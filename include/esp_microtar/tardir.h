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

// ---- Local files (this component) ---- //
#include "esp_microtar/tardir_generic.h"


#ifdef __cplusplus
extern "C" {
#endif

__attribute__((nonnull))
esp_err_t pack_dir_to_tarball(const char* dir_in, const char* tarball_out, pack_dir_to_tarball_opts_t option);

__attribute__((nonnull))
esp_err_t unpack_tarball_to_dir(const char* tarball_in, const char* output_path, unpack_tarball_to_dir_opts_t option);

#ifdef __cplusplus
}
#endif

#endif
