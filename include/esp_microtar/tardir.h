/*
 *
 */

#ifndef ESP_MICROTAR__TARDIR_H_
#define ESP_MICROTAR__TARDIR_H_

// ---- esp-idf components & system headers ---- //
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <limits.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TARDIR_DEFAULT_BUFFER_SIZE (4 * 1024)
#define TARDIR_MINIMUM_BUFFER_SIZE (PATH_MAX)

typedef struct pack_dir_to_tarball_opts {
    bool print_files;	// Unused
    bool print_dirs;	// Unused

	size_t buffer_size;	//! If set to zero, TARDIR_DEFAULT_BUFFER_SIZE will be used
} pack_dir_to_tarball_opts_t;

__attribute__((nonnull))
esp_err_t pack_dir_to_tarball(const char* dirpath, const char* output_path, pack_dir_to_tarball_opts_t option);

typedef struct unpack_tarball_to_dir_opts {
    bool print_files;	// Unused
    bool print_dirs;	// Unused

	size_t buffer_size;	//! If set to zero, TARDIR_DEFAULT_BUFFER_SIZE will be used
} unpack_tarball_to_dir_opts_t;

__attribute__((nonnull))
esp_err_t unpack_tarball_to_dir(const char* tarball, const char* output_path, unpack_tarball_to_dir_opts_t option);

#ifdef __cplusplus
}
#endif

#endif
