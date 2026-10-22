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

#include "esp_microtar/tardir_generic.h"


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
static esp_err_t pack_dir(const char* dir_in, const char* tarball_out, pack_dir_to_tarball_opts_t option);
__attribute__((nonnull))
static esp_err_t unpack_dir(const char* tarball_in, const char* output_dir, unpack_tarball_to_dir_opts_t option);

// ---- Cleanup functions ---- //
DEFINE_FREE(vfree, void*, free(_T))
DEFINE_FREE(filefree, FILE*, if (_T != NULL) fclose(_T))
DEFINE_FREE(dirfree, DIR*, if (_T != NULL) closedir(_T))
DEFINE_FREE(mtarfree, mtar_t, mtar_close(&_T))

// ---- Function definitions ---- //


esp_err_t pack_dir_to_tarball(const char* dir_in, const char* tarball_out, pack_dir_to_tarball_opts_t option)
{
	if (option.buffer_size == 0) {option.buffer_size = TARDIR_DEFAULT_BUFFER_SIZE;}
	if (option.buffer_size < TARDIR_MINIMUM_BUFFER_SIZE) {
		return ESP_ERR_INVALID_SIZE;
	}

	return pack_dir(dir_in, tarball_out, option);
}

esp_err_t unpack_tarball_to_dir(const char* tarball_in, const char* output_dir, unpack_tarball_to_dir_opts_t option)
{
	if (option.buffer_size == 0) {option.buffer_size = TARDIR_DEFAULT_BUFFER_SIZE;}
	if (option.buffer_size < TARDIR_MINIMUM_BUFFER_SIZE) {
		return ESP_ERR_INVALID_SIZE;
	}

	return unpack_dir(tarball_in, output_dir, option);
}

// ---- 'Private' function definitions ---- //



static esp_err_t pack_dir(const char* dir_in, const char* tarball_out, pack_dir_to_tarball_opts_t option)
{
	// Check arguments
	if (false == dir_exists(dir_in)) {
		ESP_LOGW(TAG, "Cannot pack '%s' : %s", dir_in, strerror(errno));
		return ESP_ERR_NOT_FOUND;
	}

	__free(vfree) char* output_dir = path_parent(tarball_out);
	if (NULL == output_dir || false == dir_exists(output_dir)) {
		ESP_LOGW(TAG, "Cannot make tarball at '%s': %s", tarball_out, "parent directory does not exists");
		return ESP_ERR_NOT_FOUND;
	}

	// Create/open tarball
	mtar_t tar;
    int tar_err = mtar_open(&tar, tarball_out, "wb");
	if (tar_err != MTAR_ESUCCESS) {
		ESP_LOGE(TAG, "Could not open '%s' for writing, error code: %d", tarball_out, tar_err);
		return ESP_FAIL;
	}

	esp_err_t ret;
	ESP_GOTO_ON_ERROR(
		pack_dir_to_tarball_generic(dir_in, &tar, option),
		delete_tar_file,
		TAG, "An error occurred, so the tarball may be invalid or corrupt. Deleting '%s'", tarball_out
	);

	
	tar_err = mtar_finalize(&tar);
	if (tar_err != MTAR_ESUCCESS) {
		ESP_LOGE(TAG, "Error while packing '%s' into '%s', error code = %d", dir_in, tarball_out, tar_err);
		return ESP_FAIL;
	}

	mtar_close(&tar);
    return ESP_OK;

delete_tar_file:
	ESP_LOGW(TAG, "An error occurred, so the tarball may be invalid or corrupt. Deleting '%s'", tarball_out);
	mtar_close(&tar);
	unlink(tarball_out);
	return ret;
}

esp_err_t unpack_dir(const char* tarball_in, const char* output_dir, unpack_tarball_to_dir_opts_t option)
{
	if (!file_exists(tarball_in)) {
		ESP_LOGW(TAG, "Cannot unpack '%s' : %s", tarball_in, strerror(errno));
		return ESP_ERR_NOT_FOUND;
	}

	if (!dir_exists(output_dir)) {
		ESP_LOGW(TAG, "No such directory '%s' : %s", output_dir, strerror(errno));
		return ESP_ERR_NOT_FOUND;
	}

	ESP_LOGD(TAG, "Unpacking contents of '%s' into '%s'", tarball_in, output_dir);

	// Create/open tarball
	__free(mtarfree)
	mtar_t tar;
    int tar_err = mtar_open(&tar, tarball_in, "rb");
	if (tar_err != MTAR_ESUCCESS) {
		ESP_LOGE(TAG, "Error while opening tarball '%s', error code = %d", tarball_in, tar_err);
		return ESP_FAIL;
	}

	return unpack_tarball_to_dir_generic(&tar, output_dir, option);
}

// Reminder to remove this later if we (or gcc) can fix the false-positives
#pragma GCC diagnostic warning "-Wcpp"
#warning "Temporarily disabled analyzer due to false-positives"
// Restore normal diagnostics
#pragma GCC diagnostic pop

