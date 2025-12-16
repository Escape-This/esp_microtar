/* Not developed with TDD!
 *
 * Instead, these are just tests of an existing open-source library. Partly to check they work,
 * but mostly as practice using it.
 */

// ---- Test framework ---- //
#include "unity.h"

// ---- Code to be tested ---- //
#include "microtar.h"

// ---- Additional test fixtures ---- //
#include "testfixture_emmc_fatfs/create_fs.h"      //!< Needed for tests involving files
#include "testfixture_emmc_fatfs/make_fresh_dir.h"

// ---- Local files ---- //

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

#include "fs_utils/snippets/file_exists.h"

// ---- Forward-declared static functions ---- //

// ---- Global variables ---- //

__attribute__((unused))
static const char* SANITY = "sanity check for test-fixture";        //!< Marks tests which are to ensure the test-fixture is working correctly (not TDD)

// Define-equivalents
#define LOCAL_TEST_DIR UNITTEST_DIR "/microtar/"

#define LOCAL_TEST_DIR_INPUT LOCAL_TEST_DIR "input/"
#define LOCAL_TEST_DIR_OUTPUT LOCAL_TEST_DIR "output/"



// ---- Cleanup functions for use with __attribute__((cleanup())) ---- //
// Used to achieve 'scope' for malloc'd memory, as if it were on the stacks, meaning
// no need to carefully free() memory before each and every return statement - brittle code
__attribute__((unused))
static void cleanup_malloc_char(char** p)
{
    free(*p);
}

__attribute__((unused))
static void cleanup_mtar(mtar_t* p)
{
    mtar_close(p);
}




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

TEST_CASE("Fail when trying to read non-existing file", "[esp_microtar][Not-TDD]")
{
    LOCAL_TEST_setUp();

    const char* filepath1 = LOCAL_TEST_DIR "notexist.lolololol";
	TEST_ASSERT_FALSE_MESSAGE(file_exists(filepath1), SANITY);

	__attribute__((cleanup(cleanup_mtar)))
	mtar_t tar;
    int err = mtar_open(&tar, filepath1, "rb");
	TEST_ASSERT_EQUAL(MTAR_EOPENFAIL, err);

    LOCAL_TEST_tearDown();
}

TEST_CASE("Writing will create the file if it does not exist", "[esp_microtar][Not-TDD]")
{
    LOCAL_TEST_setUp();

    const char* filepath1 = LOCAL_TEST_DIR "notexist.lolololol";
	TEST_ASSERT_FALSE_MESSAGE(file_exists(filepath1), SANITY);

	__attribute__((cleanup(cleanup_mtar)))
	mtar_t tar;
    int err = mtar_open(&tar, filepath1, "ab");
	TEST_ASSERT_EQUAL(MTAR_ESUCCESS, err);

	TEST_ASSERT_TRUE(file_exists(filepath1));

    LOCAL_TEST_tearDown();
}

TEST_CASE("Save and load data to tar file", "[esp_microtar][Not-TDD]")
{
    LOCAL_TEST_setUp();

    const char* tar_path = LOCAL_TEST_DIR "archive.tar";

	const char* filename[] = {
		"abcd.txt",
		"nope.avi",
		"empty.txt",
	};

	const char* filedata[] = {
		"abcd",
		"lololololol \n\n nope\n",
		"",
	};

	// Write
	{
		__attribute__((cleanup(cleanup_mtar)))
		mtar_t tar;
	    int err = mtar_open(&tar, tar_path, "ab");
		TEST_ASSERT_EQUAL(MTAR_ESUCCESS, err);
	
		for (int i=0; i < ARRAY_LENGTH(filename); i++) {
			mtar_write_file_header(&tar, filename[i], strlen(filedata[i]));
			mtar_write_data(&tar, filedata[i], strlen(filedata[i]));
		}
		mtar_finalize(&tar);
	}

	// Read file names and sizes
	{
		__attribute__((cleanup(cleanup_mtar)))
		mtar_t tar;
	    int err = mtar_open(&tar, tar_path, "rb");
		TEST_ASSERT_EQUAL(MTAR_ESUCCESS, err);

		mtar_header_t h;
		int i=0;
		while ( (mtar_read_header(&tar, &h)) != MTAR_ENULLRECORD ) {
			TEST_ASSERT_EQUAL_STRING(filename[i], h.name);
			TEST_ASSERT_EQUAL_MESSAGE(strlen(filedata[i]), h.size, filename[i]);
		  	
			mtar_next(&tar);
			i++;
		}
	}

	// Read the contents of each file
	{
		__attribute__((cleanup(cleanup_mtar)))
		mtar_t tar;
	    int err = mtar_open(&tar, tar_path, "rb");
		TEST_ASSERT_EQUAL(MTAR_ESUCCESS, err);

		char buffer[1024];
	
		for (int i=0; i < ARRAY_LENGTH(filename); i++) {
			mtar_header_t h;
			mtar_find(&tar, filename[i], &h);
			mtar_read_data(&tar, buffer, h.size);
			buffer[h.size] = 0;		// File did not have a null-terminator written to it

			TEST_ASSERT_EQUAL_STRING_MESSAGE(filedata[i], buffer, filename[i]);

		}
	}


    LOCAL_TEST_tearDown();
}






















