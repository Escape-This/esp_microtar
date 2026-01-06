/*
 *
 *
 */

// Associated header file, and extern'd global variables
#include "esp_microtar/tardir.h"

// ---- Local files ---- //
#include <stdio.h>
#include <string.h>
#include "esp_err.h"
#include "esp_check.h"
#include "esp_log.h"

#include <sys/stat.h>
#include <sys/syslimits.h>
#include <sys/unistd.h>
#include <sys/dirent.h>
#include <stdio.h>
#include "errno.h"

#include "fs_utils/filepath.h"
#include "portmacro.h"

//#include "esp_vfs.h"

// ---- Local components ---- //
#include "code_snippets/general.h"       // Useful things such as ARRAY_LENGTH() macro
#include "code_snippets/general/filepath_utils.h"

#include "fs_utils.h"

#include "fs_utils/ftree_walk.h"

#include "cwalk.h"


// ---- Local files (this component) ---- //
#include "fs_utils/snippets/file_exists.h"
#include "microtar.h"



// False-positives in this file. Hopefully this will be fixed in the future, and we can remove it
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
#pragma GCC diagnostic ignored "-Wanalyzer-file-leak"


__attribute__((unused))
static const char* TAG = "fs_utils";


// Types
typedef struct slice {
	size_t len;
	void* p;
} slice_t;

// ---- 'Private' function declarations ---- //

__attribute__((nonnull))
static esp_err_t pack_dir(const char* dirpath, const char* output_path, pack_dir_to_tarball_opts_t option);
__attribute__((nonnull))
static esp_err_t unpack_dir(const char* tarball, const char* output_dir, unpack_tarball_to_dir_opts_t option);

// ---- Cleanup functions ---- //
DEFINE_FREE(vfree, void*, free(_T))
DEFINE_FREE(filefree, FILE*, if (_T != NULL) fclose(_T))
DEFINE_FREE(dirfree, DIR*, if (_T != NULL) closedir(_T))
DEFINE_FREE(mtarfree, mtar_t, mtar_close(&_T))

// ---- Function definitions ---- //


esp_err_t pack_dir_to_tarball(const char* dirpath, const char* output_path, pack_dir_to_tarball_opts_t option)
{
	if (option.buffer_size == 0) {option.buffer_size = TARDIR_DEFAULT_BUFFER_SIZE;}
	if (option.buffer_size < TARDIR_MINIMUM_BUFFER_SIZE) {
		return ESP_ERR_INVALID_SIZE;
	}

	return pack_dir(dirpath, output_path, option);
}

esp_err_t unpack_tarball_to_dir(const char* tarball, const char* output_dir, unpack_tarball_to_dir_opts_t option)
{
	if (option.buffer_size == 0) {option.buffer_size = TARDIR_DEFAULT_BUFFER_SIZE;}
	if (option.buffer_size < TARDIR_MINIMUM_BUFFER_SIZE) {
		return ESP_ERR_INVALID_SIZE;
	}

	return unpack_dir(tarball, output_dir, option);
}

// ---- 'Private' function definitions ---- //



// Functions used by pack_dir_

esp_err_t unpack_dir__foreach(const char* tarball,
		const char* output_dir,
		unpack_tarball_to_dir_opts_t option,
		mtar_t* tar,
		const mtar_header_t h,
		char* buf,
		const size_t bufsize
)
{
	// To save memory, we use 'buf' for absolute path calculation
	assert(bufsize >= PATH_MAX);
	char* absolute_path = buf;

	size_t intended_size = 1 + cwk_path_join(output_dir, h.name, absolute_path, PATH_MAX);		
    if (intended_size > PATH_MAX) {
        errno = ENAMETOOLONG;
        return ESP_ERR_INVALID_SIZE;
    }

	// Make sure the containing directory exists first!
	mkdir_parents(absolute_path);

    __free(filefree)
	FILE* fd = fopen(absolute_path, "w");
    if (fd == NULL) {
        return ESP_FAIL;
    }

	// We're done using absolute_path, now we can use the buffer for read/write
	absolute_path = NULL;

	long position = 0;
	while(1) {
		portYIELD();	// Large files could take a while

		long remaining = h.size - position;
		if (remaining == 0) break;

		long target = (remaining > bufsize) ? bufsize : remaining;
		int tar_err = mtar_read_data(tar, buf, target);
		if (tar_err != MTAR_ESUCCESS) {
			ESP_LOGE(TAG, "Error while reading '%s' from tarball at '%s', error code = %d", h.name, tarball, tar_err);
			return ESP_FAIL;
		}

		int bytes_written = fwrite(buf, 1, target, fd);
		if (bytes_written < target) {
			// Re-calculate absolute path, so we can use it in the error message
			char* absolute_path = buf;
			cwk_path_join(output_dir, h.name, absolute_path, PATH_MAX);		
			
			ESP_LOGE(TAG, "Error while writing to '%s': %s", absolute_path, strerror(errno));
			return ESP_FAIL;
		}
		assert(bytes_written == target);

		position += target;
	}

	FILE* fd_check = no_free_ptr(fd);
    int ret = fclose(fd_check);
    if (ret != 0) {
		// Re-calculate absolute path, so we can use it in the error message
		char* absolute_path = buf;
		cwk_path_join(output_dir, h.name, absolute_path, PATH_MAX);		
        
		ESP_LOGE(TAG, "Error during fclose() of file '%s': %s", absolute_path, strerror(errno));
		return ESP_FAIL;
    }

	return ESP_OK;
}


esp_err_t unpack_dir(const char* tarball, const char* output_dir, unpack_tarball_to_dir_opts_t option)
{
	if (!file_exists(tarball)) {
		ESP_LOGW(TAG, "Cannot unpack '%s' : %s", tarball, strerror(errno));
		return ESP_ERR_NOT_FOUND;
	}

	if (!dir_exists(output_dir)) {
		ESP_LOGW(TAG, "No such directory '%s' : %s", output_dir, strerror(errno));
		return ESP_ERR_NOT_FOUND;
	}

	ESP_LOGD(TAG, "Unpacking contents of '%s' into '%s'", tarball, output_dir);

	// Allocate once, the buffer used by each callback invocation
	size_t bufsize = option.buffer_size;
    __free(vfree) char* buf = malloc(bufsize);

	// Create/open tarball
	__free(mtarfree)
	mtar_t tar;
    int tar_err = mtar_open(&tar, tarball, "rb");
	if (tar_err != MTAR_ESUCCESS) {
		ESP_LOGE(TAG, "Error while opening tarball '%s', error code = %d", tarball, tar_err);
		return ESP_FAIL;
	}

	mtar_header_t h;
	int i=0;
	while ( (mtar_read_header(&tar, &h)) != MTAR_ENULLRECORD ) {
		ESP_LOGD(TAG, "Tarball '%s' position [%d] has %u Byte file '%s'", tarball, i, h.size, h.name);

		esp_err_t err = unpack_dir__foreach(tarball, output_dir, option, &tar, h, buf, bufsize);
		ESP_RETURN_ON_ERROR(err, TAG, "unpack_dir_foreach returned error: %s", strerror(errno));

		mtar_next(&tar);
		i++;
	}

	return ESP_OK;
}

// Callbacks by pack_dir

// Context struct passed to each invocation
typedef struct {
    const pack_dir_to_tarball_opts_t option;
	const char* dirpath;

	mtar_t* tar;
	const char* output_path;	// Only used for log messages

	char* buf;
	const size_t bufsize;
} pack_dir__ctx_t;

__attribute__((nonnull))
static int pack_dir__file(const char* path, void* ctx_p)
{
    pack_dir__ctx_t* ctx = (pack_dir__ctx_t*)ctx_p;

        const char* absolute_path = path;

		// To save memory, we use 'buf' for relative path calculation
		assert(ctx->bufsize >= PATH_MAX);
		char* relative_path = ctx->buf;

		// TODO: Simplify using cwk_get_intersection()
		size_t intended_size = 1 + cwk_path_get_relative(ctx->dirpath, absolute_path, relative_path, PATH_MAX);
		assert(intended_size <= PATH_MAX);	// Should be impossible
		if (intended_size > PATH_MAX) {
	        errno = ESP_ERR_INVALID_SIZE;
	        return -1;
	    }

		ESP_LOGD(TAG, "Saving '%s' into tarball as '%s'", absolute_path, relative_path);

	    const long file_size = filesize(path);
	    if (file_size < 0) {
	        return -1;  // errno already set
	    }

		__free(filefree)
    	FILE* fd = fopen(path, "r");
	    if (fd == NULL) {
			ESP_LOGE(TAG, "Error opening: %s", absolute_path);
	        return -1;
	    }

		int tar_err;
		tar_err = mtar_write_file_header(ctx->tar, relative_path, file_size);
		if (tar_err != MTAR_ESUCCESS) {
			ESP_LOGE(TAG, "Error while writing to '%s', error code = %d", ctx->output_path, tar_err);
			return -1;
		}

		// We're done using relative_path, now we can use the buffer for read/write
		relative_path = NULL;
		size_t bufsize = ctx->bufsize;
		char* buf = ctx->buf;

		long position = 0;
		while(1) {
			portYIELD();	// Large files could take a while

			long remaining = file_size - position;
			if (remaining == 0) break;

			long target = (remaining > bufsize) ? bufsize : remaining;
			int bytes_read = fread(buf, 1, target, fd);
			if (bytes_read < target) {
				ESP_LOGE(TAG, "Error reading from: %s", absolute_path);
				return -1;
			}
			assert(bytes_read == target);

			tar_err = mtar_write_data(ctx->tar, buf, bytes_read);
			if (tar_err != MTAR_ESUCCESS) {
				ESP_LOGE(TAG, "Error while writing to '%s', error code = %d", ctx->output_path, tar_err);
				return -1;
			}

			position += target;
		}

    return 0;
}

static esp_err_t pack_dir(const char* dirpath, const char* output_path, pack_dir_to_tarball_opts_t option)
{
	// Check arguments
	if (false == dir_exists(dirpath)) {
		ESP_LOGW(TAG, "Cannot pack '%s' : %s", dirpath, strerror(errno));
		return ESP_ERR_NOT_FOUND;
	}

	if (false == dir_exists(path_parent(output_path))) {
		ESP_LOGW(TAG, "Cannot make tarball at '%s': %s", output_path, "parent directory does not exists");
		return ESP_ERR_NOT_FOUND;
	}

	// Allocate once, the buffer used by each callback invocation
	size_t bufsize = option.buffer_size;
    __free(vfree) char* buf = malloc(bufsize);

	// Create/open tarball
	mtar_t tar;
    int tar_err = mtar_open(&tar, output_path, "wb");
	if (tar_err != MTAR_ESUCCESS) {
		ESP_LOGE(TAG, "Could not open '%s' for writing, error code: %d", output_path, tar_err);
		return ESP_FAIL;
	}

    pack_dir__ctx_t callback_context = {
        .option = option,
		.dirpath = dirpath,
		.tar = &tar,
		
		.buf = buf,
		.bufsize = bufsize,
    };

    int ret = ftree_walk_ctx_simple(dirpath, &callback_context, &pack_dir__file, NULL, NULL);
    if (ret != 0) {
		ESP_LOGE(TAG, "pack_dir__file returned error: %s", strerror(errno));
		goto delete_tar_file;
    }

	tar_err = mtar_finalize(&tar);
	if (tar_err != MTAR_ESUCCESS) {
		ESP_LOGE(TAG, "Error while writing to '%s', error code = %d", output_path, tar_err);
		goto delete_tar_file;
	}

	mtar_close(&tar);

    return ESP_OK;

delete_tar_file:
	ESP_LOGW(TAG, "An error occurred, so the tarball may be invalid or corrupt. Deleting '%s'", output_path);
	mtar_close(&tar);
	unlink(output_path);
	return ESP_FAIL;
}


// Reminder to remove this later if we (or gcc) can fix the false-positives
#pragma GCC diagnostic warning "-Wcpp"
#warning "Temporarily disabled analyzer due to false-positives"
// Restore normal diagnostics
#pragma GCC diagnostic pop

