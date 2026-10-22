/*
 *
 */

#ifndef ESP_MICROTAR__TARDIR_GENERIC_H_
#define ESP_MICROTAR__TARDIR_GENERIC_H_

// ---- esp-idf components & system headers ---- //
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <limits.h>

#include "esp_err.h"

#include "microtar.h"

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

/// Add contents of directory to Tape ARchive
///
/// Recursively add each file in `dirpath` to the archive. Each entry is named after
/// its relative path from \p dir_in - calculated by \c cwk_path_get_relative()
///
/// You may run this function multiple files on the same or different directories. This
/// can result in two entries with the same name. Under the rules of TAR, the last one
/// added to the archive wins - because it overwrites the previous ones when you unpack them.
///
/// @note \p tar_out must provide callback: \p write
/// @note Does NOT call \c mtar_finalize()
///
__attribute__((nonnull))
esp_err_t pack_dir_to_tarball_generic(const char* dir_in, mtar_t *tar_out, pack_dir_to_tarball_opts_t option);

typedef struct unpack_tarball_to_dir_opts {
    bool print_files;	// Unused
    bool print_dirs;	// Unused

	size_t buffer_size;	//! If set to zero, TARDIR_DEFAULT_BUFFER_SIZE will be used
} unpack_tarball_to_dir_opts_t;


/// Unpack contents of Tape ARchive to directory
///
/// Reverse operation to \c pack_dir_to_tarball_generic(), assuming you use the same directory path.
///
/// \p tar_in must provide callbacks: \p read \p seek \p close
///
__attribute__((nonnull))
esp_err_t unpack_tarball_to_dir_generic(mtar_t *tar_in, const char* dir_out, unpack_tarball_to_dir_opts_t option);

#ifdef __cplusplus
}
#endif

#endif
