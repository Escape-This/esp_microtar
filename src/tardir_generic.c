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
#include <fcntl.h>
#include <sys/unistd.h>
#include <sys/dirent.h>
#include <stdio.h>
#include "errno.h"

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
static esp_err_t pack(const char* dir_in, mtar_t *tar_out, pack_dir_to_tarball_opts_t option);
__attribute__((nonnull))
static esp_err_t unpack(mtar_t *tar_in, const char* dir_out, unpack_tarball_to_dir_opts_t option);

// ---- Cleanup functions ---- //
DEFINE_FREE(vfree, void*, free(_T))
DEFINE_FREE(filefree, FILE*, if (_T != NULL) fclose(_T))
DEFINE_FREE(dirfree, DIR*, if (_T != NULL) closedir(_T))
DEFINE_FREE(mtarfree, mtar_t, mtar_close(&_T))
DEFINE_FREE(posix_close, int, if (_T >= 0) close(_T));
DEFINE_FREE(posix_flushclose, int, if (_T >= 0)
{
fsync(_T);
    close(_T);
});

// ---- Function definitions ---- //


esp_err_t pack_dir_to_tarball_generic(const char* dir_in, mtar_t *tar_out, pack_dir_to_tarball_opts_t option)
{
	if (option.buffer_size == 0) {option.buffer_size = TARDIR_DEFAULT_BUFFER_SIZE;}
	if (option.buffer_size < TARDIR_MINIMUM_BUFFER_SIZE) {
		return ESP_ERR_INVALID_SIZE;
	}

	return pack(dir_in, tar_out, option);
}

esp_err_t unpack_tarball_to_dir_generic(mtar_t *tar_in, const char* dir_out, unpack_tarball_to_dir_opts_t option)
{
	if (option.buffer_size == 0) {option.buffer_size = TARDIR_DEFAULT_BUFFER_SIZE;}
	if (option.buffer_size < TARDIR_MINIMUM_BUFFER_SIZE) {
		return ESP_ERR_INVALID_SIZE;
	}

	return unpack(tar_in, dir_out, option);
}

// ---- 'Private' function definitions ---- //



// Callbacks by pack_dir

// Context struct passed to each invocation
typedef struct {
    const pack_dir_to_tarball_opts_t option;
	const char* dirpath;

	mtar_t* tar;

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

		__free(posix_close)
		int fd = open(absolute_path, O_RDONLY);
		if (fd < 0) {
			ESP_LOGW(TAG, "cannot open file %s : %s", absolute_path, strerror(errno));
			return -1;
		}

		int tar_err;
		tar_err = mtar_write_file_header(ctx->tar, relative_path, file_size);
		if (tar_err != MTAR_ESUCCESS) {
			ESP_LOGE(TAG, "Error writing tarball, error code = %d", tar_err);
			return -1;
		}

		// We're done using relative_path, now we can use the buffer for read/write
		relative_path = NULL;
		size_t buffer_size = ctx->bufsize;
		char* buffer = ctx->buf;

		while (1) {
			if (ctx->option.yieldFn) ctx->option.yieldFn(); // Large files could take a while

			int n_read = read(fd, buffer, buffer_size);
			if (n_read < 0) {
				perror("read error");
				return -1;
			}

			if (n_read == 0) {
				// No more data
				break;
			}

			tar_err = mtar_write_data(ctx->tar, buffer, n_read);
			if (tar_err != MTAR_ESUCCESS) {
				ESP_LOGE(TAG, "Error writing tarball, error code = %d", tar_err);
				return -1;
			}
		}

    return 0;
}

static esp_err_t pack(const char* dir_in, mtar_t *tar_out, pack_dir_to_tarball_opts_t option)
{
	// Check arguments
	if (false == dir_exists(dir_in)) {
		ESP_LOGW(TAG, "Cannot pack '%s' : %s", dir_in, strerror(errno));
		return ESP_ERR_NOT_FOUND;
	}

	// Allocate once, the buffer used by each callback invocation
	size_t bufsize = option.buffer_size;
    __free(vfree) char* buf = malloc(bufsize);

    pack_dir__ctx_t callback_context = {
        .option = option,
		.dirpath = dir_in,
		.tar = tar_out,
		
		.buf = buf,
		.bufsize = bufsize,
    };

    int ret = ftree_walk_ctx_simple(dir_in, &callback_context, &pack_dir__file, NULL, NULL);
    if (ret != 0) {
		ESP_LOGE(TAG, "pack_dir__file returned error: %s", strerror(errno));
		return ESP_FAIL;
    }

    return ESP_OK;
}


// Callbacks by unpack_dir


esp_err_t unpack_dir__foreach(
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

    __free(posix_flushclose)
    int fd = open(absolute_path, (O_WRONLY | O_CREAT | O_TRUNC));
    ESP_RETURN_ON_FALSE(fd >= 0, -1, TAG, "failed to open '%s' for writing: %s", absolute_path, strerror(errno));


	// We're done using absolute_path, now we can use the buffer for read/write
	absolute_path = NULL;

	long position = 0;
	while(1) {
		long remaining = h.size - position;
		if (remaining == 0) break;

		long target = (remaining > bufsize) ? bufsize : remaining;
		int tar_err = mtar_read_data(tar, buf, target);
		if (tar_err != MTAR_ESUCCESS) {
			ESP_LOGE(TAG, "Error while reading '%s' from tarball, error code = %d", h.name, tar_err);
			return ESP_FAIL;
		}

		int todo = target;
        char* finger = buf;
		while (todo > 0) {
			if (option.yieldFn) option.yieldFn();

            int n_written = write(fd, finger, todo);
			if (n_written < 0) {
				// Re-calculate absolute path, so we can use it in the error message
				char* absolute_path = buf;
				cwk_path_join(output_dir, h.name, absolute_path, PATH_MAX);	
				ESP_LOGE(TAG, "Error while writing to '%s': %s", absolute_path, strerror(errno));
				return ESP_FAIL;
			}
            todo -= n_written;
            finger += n_written;
        }

		position += target;
	}

	return ESP_OK;
}


static esp_err_t unpack(mtar_t *tar_in, const char* dir_out, unpack_tarball_to_dir_opts_t option)
{
	ESP_LOGD(TAG, "Unpacking tarball to '%s'", dir_out);

	// Allocate once, the buffer used by each callback invocation
	size_t bufsize = option.buffer_size;
    __free(vfree) char* buf = malloc(bufsize);

	mtar_header_t h;
	int i=0;
	while ( (mtar_read_header(tar_in, &h)) != MTAR_ENULLRECORD ) {
		ESP_LOGD(TAG, "Tarball position [%d] has %u Byte file '%s'", i, h.size, h.name);

		esp_err_t err = unpack_dir__foreach(dir_out, option, tar_in, h, buf, bufsize);
		ESP_RETURN_ON_ERROR(err, TAG, "unpack_dir_foreach returned error: %s", strerror(errno));

		mtar_next(tar_in);
		i++;
	}

	return ESP_OK;
}


// Reminder to remove this later if we (or gcc) can fix the false-positives
#pragma GCC diagnostic warning "-Wcpp"
#warning "Temporarily disabled analyzer due to false-positives"
// Restore normal diagnostics
#pragma GCC diagnostic pop

