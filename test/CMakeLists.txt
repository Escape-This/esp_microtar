idf_component_register(

	SRC_DIRS "."

	INCLUDE_DIRS "."

    REQUIRES unity esp_microtar

    PRIV_REQUIRES
    	code_snippets
		fs_utils

		esp_timer
	    json

    # This component sets up eMMC memory/filesystem
    PRIV_REQUIRES testfixture_emmc_fatfs

)
