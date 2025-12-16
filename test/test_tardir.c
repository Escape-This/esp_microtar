/* Not developed with TDD!
 *
 * Instead, these are just tests of an existing open-source library. Partly to check they work,
 * but mostly as practice using it.
 */

// ---- Test framework ---- //
#include "unity.h"

// ---- Code to be tested ---- //
#include "esp_microtar/tardir.h"

// ---- Additional test fixtures ---- //
#include "testfixture_emmc_fatfs/create_fs.h"      //!< Needed for tests involving files
#include "testfixture_emmc_fatfs/make_fresh_dir.h"

// ---- esp-idf components & system headers ---- //
//#include <sys/param.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <sys/syslimits.h>

#include <errno.h>

#include "esp_system.h"  // Allows measuring currently available memory

// ---- Local components ---- //
#include "code_snippets/general.h"       // Useful things such as ARRAY_LENGTH() macro
#include "code_snippets/general/filepath_utils.h"

#include "fs_utils.h"

// ---- Local files (this component) ---- //
#include "microtar.h"


// ---- Forward-declared static functions ---- //

// ---- Global variables ---- //

__attribute__((unused))
static const char* SANITY = "sanity check for test-fixture";        //!< Marks tests which are to ensure the test-fixture is working correctly (not TDD)

// Define-equivalents
#define LOCAL_TEST_DIR UNITTEST_DIR "/tardir/"

#define LOCAL_TEST_DIR_INPUT LOCAL_TEST_DIR "input/"
#define LOCAL_TEST_DIR_OUTPUT LOCAL_TEST_DIR "output/"



// ---- Cleanup functions ---- //
DEFINE_FREE(vfree, void*, free(_T))



// This will run before each test!
static void LOCAL_TEST_setUp(void)
{
    // Setup and mount the eMMC card, if it isn't already
    setup_emmc_for_tests();

    // prepare empty directories
    make_fresh_dir(LOCAL_TEST_DIR);
    make_fresh_dir(LOCAL_TEST_DIR_INPUT);
    make_fresh_dir(LOCAL_TEST_DIR_OUTPUT);

    errno = 0;
}

// This will run after each test!
static void LOCAL_TEST_tearDown(void)
{
    ;
}

TEST_CASE("Function prototype: pack_dir_to_tarball()", "[esp_microtar]")
{
    LOCAL_TEST_setUp();

    // Function arguments have attribute __nonnull__, so we need to pass it something...
    const char* emptystr = "";
    pack_dir_to_tarball_opts_t options = {0};

    // Input dir path, output file path, options
    pack_dir_to_tarball(emptystr, emptystr, options);

    LOCAL_TEST_tearDown();
}

TEST_CASE("Function prototype: unpack_tarball_to_dir()", "[esp_microtar]")
{
    LOCAL_TEST_setUp();

    // Function arguments have attribute __nonnull__, so we need to pass it something...
    const char* emptystr = "";
    unpack_tarball_to_dir_opts_t options = {0};

    // Input file path, output dir path, options
    unpack_tarball_to_dir(emptystr, emptystr, options);

    LOCAL_TEST_tearDown();
}


TEST_CASE("Tar and untar a single file", "[esp_microtar]")
{
    LOCAL_TEST_setUp();

	const char* tarball_path = LOCAL_TEST_DIR "test.tar";
	
	// Create file
	const char* input_file = LOCAL_TEST_DIR_INPUT "file.txt";
	const char* filedata = "abcdef";
	save_text(input_file, filedata);
	const char* expected_out_file  = LOCAL_TEST_DIR_OUTPUT "file.txt";
	TEST_ASSERT_FALSE_MESSAGE(file_exists(expected_out_file), SANITY);

	// Make tarball
	{
	    pack_dir_to_tarball_opts_t options = {0};
	    esp_err_t err = pack_dir_to_tarball(LOCAL_TEST_DIR_INPUT, tarball_path, options);
		TEST_ASSERT_EQUAL(ESP_OK, err);
	}

	TEST_ASSERT_TRUE(file_exists(tarball_path));
	
	// Unpack tarball
	{
	    unpack_tarball_to_dir_opts_t options = {0};
	    esp_err_t err = unpack_tarball_to_dir(tarball_path, LOCAL_TEST_DIR_OUTPUT, options);
		TEST_ASSERT_EQUAL(ESP_OK, err);
	}

	TEST_ASSERT_TRUE(file_exists(expected_out_file));

	TEST_ASSERT_EQUAL(0, diff_file(input_file, expected_out_file));

    LOCAL_TEST_tearDown();
}

TEST_CASE("Tar and untar binary files", "[esp_microtar]")
{
    LOCAL_TEST_setUp();

	const char* tarball_path = LOCAL_TEST_DIR "binary.tar";
	
	// Create file
	const char* input_file = LOCAL_TEST_DIR_INPUT "int32.bin";
	uint32_t test_data = 0x01234567;
	save_binary_file(input_file, &test_data, sizeof(test_data));
	TEST_ASSERT_EQUAL_MESSAGE(4, filesize(input_file), SANITY);

	const char* expected_out_file  = LOCAL_TEST_DIR_OUTPUT "int32.bin";
	TEST_ASSERT_FALSE_MESSAGE(file_exists(expected_out_file), SANITY);

	// Make tarball
	{
	    pack_dir_to_tarball_opts_t options = {0};
	    esp_err_t err = pack_dir_to_tarball(LOCAL_TEST_DIR_INPUT, tarball_path, options);
		TEST_ASSERT_EQUAL(ESP_OK, err);
	}

	TEST_ASSERT_TRUE(file_exists(tarball_path));
	
	// Unpack tarball
	{
	    unpack_tarball_to_dir_opts_t options = {0};
	    esp_err_t err = unpack_tarball_to_dir(tarball_path, LOCAL_TEST_DIR_OUTPUT, options);
		TEST_ASSERT_EQUAL(ESP_OK, err);
	}

	TEST_ASSERT_TRUE(file_exists(expected_out_file));

	// Load and compare
	{
        __free(vfree)
        char* memory;
        size_t len;
        int ret = load_binary_file(expected_out_file, &memory, &len);
        TEST_ASSERT_EQUAL_MESSAGE(0, ret, SANITY);

        TEST_ASSERT_EQUAL(4, len);
        TEST_ASSERT_NOT_NULL(memory);
        TEST_ASSERT_EQUAL_HEX32(test_data, *(uint32_t*)memory);	// This tells us what the difference is
	}

	TEST_ASSERT_EQUAL(0, diff_file(input_file, expected_out_file));

    LOCAL_TEST_tearDown();
}

TEST_CASE("Fail if tarball does not exist", "[esp_microtar]")
{
    LOCAL_TEST_setUp();

	// Empty file for tarball, or else it will return a different error;
	const char* tarball_path = LOCAL_TEST_DIR "test.tar";

    const char* dirpath = LOCAL_TEST_DIR_INPUT;
    
	unpack_tarball_to_dir_opts_t options = {0};
    esp_err_t err = unpack_tarball_to_dir(tarball_path, dirpath, options);

	TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, err);

    LOCAL_TEST_tearDown();
}

TEST_CASE("Fail if tarball points to a directory", "[esp_microtar]")
{
    LOCAL_TEST_setUp();

	// Empty file for tarball, or else it will return a different error;
	const char* tarball_path = LOCAL_TEST_DIR "/lololol/nosuchfile";
	make_fresh_dir(tarball_path);

    const char* dirpath = LOCAL_TEST_DIR_INPUT;
    
	unpack_tarball_to_dir_opts_t options = {0};
    esp_err_t err = unpack_tarball_to_dir(tarball_path, dirpath, options);

	TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, err);

    LOCAL_TEST_tearDown();
}


TEST_CASE("Fail if target directory does not exist", "[esp_microtar]")
{
    LOCAL_TEST_setUp();

	// Empty file for tarball, or else it will return a different error;
	const char* tarball_path = LOCAL_TEST_DIR "test.tar";

    const char* dirpath = LOCAL_TEST_DIR "/lololol/nosuchdir";
    
	unpack_tarball_to_dir_opts_t options = {0};
    esp_err_t err = unpack_tarball_to_dir(tarball_path, dirpath, options);

	TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, err);

    LOCAL_TEST_tearDown();
}

TEST_CASE("Fail if target directory points to a file", "[esp_microtar]")
{
    LOCAL_TEST_setUp();

	// Empty file for tarball, or else it will return a different error;
	const char* tarball_path = LOCAL_TEST_DIR "test.tar";
	touch(tarball_path);

    const char* dirpath = LOCAL_TEST_DIR_OUTPUT "notadirectory";
	touch(dirpath);

    unpack_tarball_to_dir_opts_t options = {0};
    esp_err_t err = unpack_tarball_to_dir(tarball_path, dirpath, options);

	TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, err);

    LOCAL_TEST_tearDown();
}


TEST_CASE("Tar and untar multiple files", "[esp_microtar][Not-TDD]")
{
    LOCAL_TEST_setUp();

    // Function arguments have attribute __nonnull__, so we need to pass it something...
    const char* emptystr = "";

	const char* tarball_path = LOCAL_TEST_DIR "test.tar";
	
	// Create files
	const char* filename[] = {
		LOCAL_TEST_DIR_INPUT "abcd.txt",
		LOCAL_TEST_DIR_INPUT "nope.avi",
		LOCAL_TEST_DIR_INPUT "empty.txt",
	};

	const char* filedata[] = {
		"abcdefghijk",
		"lololololol \n\n nope\n kk haha \n",
		"",
	};

	for (int i=0; i < ARRAY_LENGTH(filename); i++) {
		save_text(filename[i], filedata[i]);
	}


	const char* expected_out_file[] = {
		LOCAL_TEST_DIR_OUTPUT "abcd.txt",
		LOCAL_TEST_DIR_OUTPUT "nope.avi",
		LOCAL_TEST_DIR_OUTPUT "empty.txt",
	};
	// Make tarball
	{
	    pack_dir_to_tarball_opts_t options = {0};
	    esp_err_t err = pack_dir_to_tarball(LOCAL_TEST_DIR_INPUT, tarball_path, options);
		TEST_ASSERT_EQUAL(ESP_OK, err);
	}

	TEST_ASSERT_TRUE(file_exists(tarball_path));
	
	// Unpack tarball
	{
	    unpack_tarball_to_dir_opts_t options = {0};
	    esp_err_t err = unpack_tarball_to_dir(tarball_path, LOCAL_TEST_DIR_OUTPUT, options);
		TEST_ASSERT_EQUAL(ESP_OK, err);
	}

	for (int i=0; i < ARRAY_LENGTH(expected_out_file); i++) {
		TEST_ASSERT_TRUE(file_exists(expected_out_file[i]));
		TEST_ASSERT_EQUAL(0, diff_file(filename[i], expected_out_file[i]));

	}

    LOCAL_TEST_tearDown();
}

TEST_CASE("Recurse into subdirectories", "[esp_microtar][TDD-dev]")
{
    LOCAL_TEST_setUp();

	const char* tarball_path = LOCAL_TEST_DIR "test.tar";
	
	// Create file in a subdirectory
	const char* subdir = LOCAL_TEST_DIR_INPUT "subdir/";
	make_fresh_dir(subdir);
	const char* input_file = LOCAL_TEST_DIR_INPUT "subdir/" "file.txt";
	const char* filedata = "abcdef";
	save_text(input_file, filedata);

	// Output should likewise be in a subdirectory
	const char* expected_subdir = LOCAL_TEST_DIR_OUTPUT "subdir/";
	const char* expected_out_file = LOCAL_TEST_DIR_OUTPUT "subdir/"  "file.txt";
	TEST_ASSERT_FALSE_MESSAGE(file_exists(expected_out_file), SANITY);

	// Make tarball
	{
	    pack_dir_to_tarball_opts_t options = {0};
	    esp_err_t err = pack_dir_to_tarball(LOCAL_TEST_DIR_INPUT, tarball_path, options);
		TEST_ASSERT_EQUAL(ESP_OK, err);
	}

	TEST_ASSERT_TRUE(file_exists(tarball_path));

	// Verify that the subdirectory does not exist yet
	TEST_ASSERT_FALSE_MESSAGE(dir_exists(expected_subdir), SANITY);
	
	// Unpack tarball
	{
	    unpack_tarball_to_dir_opts_t options = {0};
	    esp_err_t err = unpack_tarball_to_dir(tarball_path, LOCAL_TEST_DIR_OUTPUT, options);
		TEST_ASSERT_EQUAL(ESP_OK, err);
	}

	TEST_ASSERT_TRUE(dir_exists(expected_subdir));
	TEST_ASSERT_TRUE(file_exists(expected_out_file));

	TEST_ASSERT_EQUAL(0, diff_file(input_file, expected_out_file));

    LOCAL_TEST_tearDown();
}














