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
//#include "esp_vfs.h"

#include "errno.h"

// ---- Local components ---- //
#include "code_snippets/general.h"       // Useful things such as ARRAY_LENGTH() macro
#include "code_snippets/general/filepath_utils.h"

#include "fs_utils.h"

#include "fs_utils/ftree_walk.h"

#include "cwalk.h"


// ---- Local files (this component) ---- //
#include "microtar.h"


__attribute__((unused))
static const char* TAG = "fs_utils";


// Types
typedef struct slice {
	size_t len;
	void* p;
} slice_t;

// ---- 'Private' function declarations ---- //

__attribute__((nonnull))
static esp_err_t pack_dir_to_tarball_recursive(const char* dirpath, const char* output_path, pack_dir_to_tarball_opts_t option);

// ---- Cleanup functions ---- //
DEFINE_FREE(vfree, void*, free(_T))
DEFINE_FREE(dirfree, DIR*, closedir(_T))
DEFINE_FREE(mtarfree, mtar_t, mtar_close(&_T))

__attribute__((unused))
static void cleanup_slice_t(slice_t* slice)
{
    free(slice->p);
}

// ---- Function definitions ---- //


esp_err_t pack_dir_to_tarball(const char* dirpath, const char* output_path, pack_dir_to_tarball_opts_t option)
{
	return pack_dir_to_tarball_recursive(dirpath, output_path, option);
}

esp_err_t unpack_tarball_to_dir(const char* tarball, const char* output_dir, unpack_tarball_to_dir_opts_t option)
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

    __free(vfree)
	char* buffer1 = malloc(PATH_MAX);
	if (NULL == buffer1) {
		return ESP_ERR_NO_MEM;
	}

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

		size_t intended_size = 1 + cwk_path_join(output_dir, h.name, buffer1, PATH_MAX);		
	    if (intended_size > PATH_MAX) {
	        errno = ENAMETOOLONG;
	        return ESP_ERR_INVALID_SIZE;
	    }

		// Alternate, more descriptive name
		const char* absolute_path = buffer1;

   		__free(vfree)
		char* buffer = malloc(h.size);
		if (NULL == buffer) {
			return ESP_ERR_NO_MEM;
		}


		int tar_err = mtar_read_data(&tar, buffer, h.size);
		if (tar_err != MTAR_ESUCCESS) {
			ESP_LOGE(TAG, "Error while reading '%s' from tarball at '%s', error code = %d", h.name, tarball, tar_err);
			return ESP_FAIL;
		}

//		esp_err_t err = save_memory_to_file(buffer, h.size, absolute_path);
//		ESP_RETURN_ON_ERROR(err, TAG, "Could not write to '%s'", absolute_path);
    	int ret = save_binary_file(absolute_path, buffer, h.size);
		if (ret != 0) {
			if (errno == ENOENT) {
				// Directory does not yet exist, create them and try again
				mkdir_parents(absolute_path);
				ret = save_binary_file(absolute_path, buffer, h.size);
			}
		}
		if (ret != 0) {
			ESP_LOGE(TAG, "Failed to write file to '%s' : %s", absolute_path, strerror(errno));
			return ESP_FAIL;
		}
		
		mtar_next(&tar);
		i++;
	}

	return ESP_OK;
}


// ---- 'Private' function definitions ---- //





// Callbacks by pack_dir_to_tarball_recursive

// Context struct passed to each invocation
typedef struct {
    const pack_dir_to_tarball_opts_t option;
	const char* dirpath;

	mtar_t* tar;
	const char* output_path;	// Only used for log messages

	char* path_buffer;
} pack_dir_to_tarball_recursive__ctx_t;

__attribute__((nonnull))
static int pack_dir_to_tarball_recursive__file(const char* path, void* ctx_p)
{
    pack_dir_to_tarball_recursive__ctx_t* ctx = (pack_dir_to_tarball_recursive__ctx_t*)ctx_p;

        const char* absolute_path = path;

		// TODO: Simplify using cwk_get_intersection()
		size_t intended_size = 1 + cwk_path_get_relative(ctx->dirpath, absolute_path, ctx->path_buffer, PATH_MAX);
		assert(intended_size <= PATH_MAX);	// Should be impossible
		if (intended_size > PATH_MAX) {
	        errno = ESP_ERR_INVALID_SIZE;
	        return EINVAL;
	    }

		// Alternate, more descriptive name
		const char* relative_path = ctx->path_buffer;

		ESP_LOGD(TAG, "Saving '%s' into tarball as '%s'", absolute_path, relative_path);

//		// Load the contents of the file
//		__attribute__((cleanup(cleanup_slice_t)))
//		slice_t file_data = load_file_into_memory(absolute_path);
//		if (NULL == file_data.p) {
//			ESP_LOGE(TAG, "Could not load file into memory: %s", strerror(errno));
//			return ESP_ERR_NO_MEM;
//		}
//		ESP_LOGD(TAG, "Loaded %d Bytes of memory from '%s'", file_data.len, absolute_path);
	    __free(vfree)
	    char* memory;
	    size_t len;
	    int ret = load_binary_file(absolute_path, &memory, &len);
		if (ret != 0) {
			ESP_LOGE(TAG, "Failed to load file at '%s' : %s", absolute_path, strerror(errno));
		}

		int tar_err;
		tar_err = mtar_write_file_header(ctx->tar, relative_path, len);
		if (tar_err != MTAR_ESUCCESS) {
			ESP_LOGE(TAG, "Error while writing to '%s', error code = %d", ctx->output_path, tar_err);
			return -1;
		}
		tar_err = mtar_write_data(ctx->tar, memory, len);
		if (tar_err != MTAR_ESUCCESS) {
			ESP_LOGE(TAG, "Error while writing to '%s', error code = %d", ctx->output_path, tar_err);
			return -1;
		}


    return 0;
}

//__attribute__((nonnull))
//int pack_dir_to_tarball_recursive__pre_dir(const char* path, void* ctx_p)
//{
//    pack_dir_to_tarball_recursive__ctx_t* ctx = (pack_dir_to_tarball_recursive__ctx_t*)ctx_p;
//
//    ctx->result->n_dirs += 1;
//
//    // Counters are only used when printing directories
//    if (ctx->counter != NULL) {
//        ctx->counter[ctx->current_depth] = 0;
//    }
//
//    ctx->current_depth += 1;
//    if (ctx->current_depth > ctx->result->peak_depth) {
//        ctx->result->peak_depth = ctx->current_depth;
//    }
//
//    return 0;
//}
//
//__attribute__((nonnull))
//int pack_dir_to_tarball_recursive__post_dir(const char* path, void* ctx_p)
//{
//    pack_dir_to_tarball_recursive__ctx_t* ctx = (pack_dir_to_tarball_recursive__ctx_t*)ctx_p;
//
//    assert(ctx->current_depth > 0);
//    ctx->current_depth -= 1;
//
//    // Counters are only used when printing directories
//    if (ctx->counter != NULL) {
//        if (ctx-> current_depth <= ctx->option.max_depth) {
//            fprintf(ctx->output, "%"PRIi64" %s\n", ctx->counter[ctx->current_depth], path);
//        }
//
//        // Add count to parent directory
//        if (ctx->current_depth > 0) {
//            ctx->counter[ctx->current_depth - 1] += ctx->counter[ctx->current_depth];
//        }
//    }
//
//    return 0;
//}

static esp_err_t pack_dir_to_tarball_recursive(const char* dirpath, const char* output_path, pack_dir_to_tarball_opts_t option)
{

    __free(vfree)
	char* buffer1 = malloc(PATH_MAX);
	if (NULL == buffer1) {
		return ESP_ERR_NO_MEM;
	}

	// Create/open tarball
	mtar_t tar;
    int tar_err = mtar_open(&tar, output_path, "wb");
	if (tar_err != MTAR_ESUCCESS) {
		ESP_LOGE(TAG, "Could not open '%s' for writing, error code: %d", output_path, tar_err);
		return ESP_FAIL;
	}



    pack_dir_to_tarball_recursive__ctx_t callback_context = {
        .option = option,
		.dirpath = dirpath,
		.tar = &tar,
		
		.path_buffer = buffer1,
    };

    int ret = ftree_walk_ctx_simple(dirpath, &callback_context, &pack_dir_to_tarball_recursive__file, NULL, NULL);
    if (ret != 0) {
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
