idf_component_register(

	# Put headers in PRIV_INCLUDE where possible
#	PRIV_INCLUDE_DIRS "priv_include"
	INCLUDE_DIRS "include"

	SRCS "src/microtar.c"

   	# files whose headers are in priv_include (only used in this component)


	###########################

	REQUIRES

	PRIV_REQUIRES


)

# Append compiler options, for this component only!

target_compile_options(${COMPONENT_LIB} PRIVATE -Werror)	# Doesn't work?
target_compile_options(${COMPONENT_LIB} PRIVATE -fanalyzer)
