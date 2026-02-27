#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "sparkcan::sparkcan" for configuration "Release"
set_property(TARGET sparkcan::sparkcan APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(sparkcan::sparkcan PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libsparkcan.so"
  IMPORTED_SONAME_RELEASE "libsparkcan.so"
  )

list(APPEND _cmake_import_check_targets sparkcan::sparkcan )
list(APPEND _cmake_import_check_files_for_sparkcan::sparkcan "${_IMPORT_PREFIX}/lib/libsparkcan.so" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
