/* Not developed with TDD!
 *
 * Instead, these are just tests of an existing open-source library. Partly to check they work,
 * but mostly as practice using it.
 */

// ---- Test framework ---- //
#include "esp_err.h"
#include "fs_utils/load_file.h"
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

#include "esp_log.h"
#include "esp_system.h"  // Allows measuring currently available memory

// ---- Local components ---- //
#include "code_snippets/general.h"       // Useful things such as ARRAY_LENGTH() macro
#include "code_snippets/general/filepath_utils.h"

#include "fs_utils.h"

// ---- Local files (this component) ---- //
#include "microtar.h"


// ---- Forward-declared static functions ---- //

// ---- Global variables ---- //

// Tag prepended to ESP_LOG messages in this component: can also be used to filter messages at runtime
__attribute__((unused))
static const char* TAG = "test_tardir";

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
	TEST_ASSERT_TRUE_MESSAGE(file_exists(input_file), SANITY);

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

TEST_CASE("Recurse into subdirectories", "[esp_microtar]")
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


TEST_CASE("Use O(1) memory, regardless of file sizes", "[esp_microtar]")
{
    LOCAL_TEST_setUp();

	size_t fsize_big = 32 * 1024;
	size_t fsize_bigger = 48 * 1024;
	size_t fsize_difference = (fsize_bigger - fsize_big);

	/* 	This requires some explanation
	 *	Basically, we need a simple way to check that the function is using a scratch buffer,
	 * 	not loading whole files into RAM before writing the. The options are:
     *		- Add hooks in the memory system (hard)
	 *		- Make sure there's no space in memory for a massive block (could take a while, especially for PSRAM!)
	 *		- Measure the heap high-water mark, and see if it's dropped substantially
	 *	The latter seems easiest, but will only work if the current heap is close to high-water mark
	 * 	This code makes sure that is case.
	 */
	__free(vfree)
	void* squat_on_memory = NULL;
	{
		uint32_t depth = esp_get_free_heap_size() - esp_get_minimum_free_heap_size();
		ESP_LOGD(TAG, "Before: current=%u, min=%u, difference=%u", (uint)esp_get_free_heap_size(), (uint)esp_get_minimum_free_heap_size(), (uint)depth);
	
		if (depth > 1024) {
			squat_on_memory = malloc(depth - 1024);
			TEST_ASSERT_NOT_NULL_MESSAGE(squat_on_memory, "failed to malloc, test cannot proceed");
		}
		
		depth = esp_get_free_heap_size() - esp_get_minimum_free_heap_size();
		ESP_LOGD(TAG, "After: current=%u, min=%u, difference=%u", (uint)esp_get_free_heap_size(), (uint)esp_get_minimum_free_heap_size(), (uint)depth);
		
		// The difference will still be > 500 or therabouts, simply due to background stuff
		// As long as it's smaller than the difference in file size's
		TEST_ASSERT_LESS_THAN_MESSAGE((fsize_difference / 2), depth, "heap minimum_free is too low, test cannot proceed");
	}

	const char* tarball_path = LOCAL_TEST_DIR "binary.tar";

	size_t set_buffer_size = 4 * 1024;	// Used to control the buffer sizes used
	TEST_ASSERT_LESS_THAN_MESSAGE(fsize_big, set_buffer_size, "buffer should be smaller than the file, for the test to work");
	
	
	// Create large-ish file
	{
		const char* input_file = LOCAL_TEST_DIR_INPUT "big_file.bin";
		uint32_t test_data = 0x01234567;
		uint32_t buffer[1024/4];
		for (int i=0; i < ARRAY_LENGTH(buffer); i++) {
			buffer[i] = test_data;
		}
		for (int i=0; i < (fsize_big); i += sizeof(buffer)) {
			append_binary_file(input_file, &buffer, sizeof(buffer));
		}
		TEST_ASSERT_EQUAL_MESSAGE(fsize_big, filesize(input_file), SANITY);
	
		const char* expected_out_file  = LOCAL_TEST_DIR_OUTPUT "big_file.bin";
		TEST_ASSERT_FALSE_MESSAGE(file_exists(expected_out_file), SANITY);
	
		// Make tarball
		{
		    pack_dir_to_tarball_opts_t options = {
				.buffer_size = set_buffer_size,	
			};
		    esp_err_t err = pack_dir_to_tarball(LOCAL_TEST_DIR_INPUT, tarball_path, options);
			TEST_ASSERT_EQUAL(ESP_OK, err);
		}
	
		TEST_ASSERT_TRUE(file_exists(tarball_path));
		
		// Unpack tarball
		{
		    unpack_tarball_to_dir_opts_t options = {
				.buffer_size = set_buffer_size,	
};
		    esp_err_t err = unpack_tarball_to_dir(tarball_path, LOCAL_TEST_DIR_OUTPUT, options);
			TEST_ASSERT_EQUAL(ESP_OK, err);
		}

		// Check it's the same file
		TEST_ASSERT_EQUAL_MESSAGE(0, diff_file(input_file, expected_out_file), SANITY);
	}

	uint32_t min_free_1 = esp_get_minimum_free_heap_size();

	// Repeat with a larger file (44kB)
	{
		const char* input_file = LOCAL_TEST_DIR_INPUT "even_bigger_file.bin";
		uint32_t test_data = 0x01234567;
		uint32_t buffer[1024/4];
		for (int i=0; i < ARRAY_LENGTH(buffer); i++) {
			buffer[i] = test_data;
		}
		for (int i=0; i < (fsize_bigger); i += sizeof(buffer)) {
			append_binary_file(input_file, &buffer, sizeof(buffer));
		}
		TEST_ASSERT_EQUAL_MESSAGE(fsize_bigger, filesize(input_file), SANITY);
	
		const char* expected_out_file  = LOCAL_TEST_DIR_OUTPUT "even_bigger_file.bin";
		TEST_ASSERT_FALSE_MESSAGE(file_exists(expected_out_file), SANITY);
	
		// Make tarball
		{
		    pack_dir_to_tarball_opts_t options = {
				.buffer_size = set_buffer_size,	
};
		    esp_err_t err = pack_dir_to_tarball(LOCAL_TEST_DIR_INPUT, tarball_path, options);
			TEST_ASSERT_EQUAL(ESP_OK, err);
		}
	
		TEST_ASSERT_TRUE(file_exists(tarball_path));
		
		// Unpack tarball
		{
		    unpack_tarball_to_dir_opts_t options = {
				.buffer_size = set_buffer_size,	
          };
		    esp_err_t err = unpack_tarball_to_dir(tarball_path, LOCAL_TEST_DIR_OUTPUT, options);
			TEST_ASSERT_EQUAL(ESP_OK, err);
		}

		// Check it's the same file
		TEST_ASSERT_EQUAL_MESSAGE(0, diff_file(input_file, expected_out_file), SANITY);
	}


	uint32_t min_free_2 = esp_get_minimum_free_heap_size();	// always <= the previous reading
	uint32_t difference = min_free_1 - min_free_2;	// always positive
	
	// Min should be about the same, should definitely not be 16kB smaller. Pick a medium, in case of 'noise'
	TEST_ASSERT_LESS_THAN((fsize_difference / 2), difference);	// Not tight enough to be reliable
	TEST_ASSERT_LESS_THAN(2048, difference);	// Tighter test, in case the noise gets larger
	TEST_ASSERT_LESS_THAN(512, difference);		// Possibly too tight of a test
	

    LOCAL_TEST_tearDown();
}



TEST_CASE("Memory size option must be either 0, or above a minimum amount", "[esp_microtar]")
{
    LOCAL_TEST_setUp();

	const char* tarball_path = LOCAL_TEST_DIR "test.tar";
	
	// Create file
	const char* input_file = LOCAL_TEST_DIR_INPUT "file.txt";
	const char* filedata = "abcdef";
	save_text(input_file, filedata);
	TEST_ASSERT_TRUE_MESSAGE(file_exists(input_file), SANITY);

	const char* expected_out_file  = LOCAL_TEST_DIR_OUTPUT "file.txt";
	TEST_ASSERT_FALSE_MESSAGE(file_exists(expected_out_file), SANITY);

	// Make tarball
	{
	    pack_dir_to_tarball_opts_t options = {
			.buffer_size = 10,
		};
	    esp_err_t err = pack_dir_to_tarball(LOCAL_TEST_DIR_INPUT, tarball_path, options);
		TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE, err);
	}
	
	// Unpack tarball
	{
	    unpack_tarball_to_dir_opts_t options = {
			.buffer_size = 10,
		};
	    esp_err_t err = unpack_tarball_to_dir(tarball_path, LOCAL_TEST_DIR_OUTPUT, options);
		TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE, err);
	}

    LOCAL_TEST_tearDown();
}







