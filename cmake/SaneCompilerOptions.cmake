if(MSVC)
  add_compile_options(
    "/utf-8"
    "$<$<COMPILE_LANGUAGE:CXX>:/Zc:__cplusplus>")
endif()

if(WIN32)
  add_definitions(
    "-DNOMINMAX"
    "-D_USE_MATH_DEFINES"
    "-D_CRT_SECURE_NO_WARNINGS")
else()
  add_definitions(
    "-D_FILE_OFFSET_BITS=64")
endif()

add_library(sane-warning-flags INTERFACE)
if(NOT MSVC)
  target_compile_options(sane-warning-flags INTERFACE
    "-Wall"
    "-Wextra"
    "-Werror=return-type"
    "-Wno-multichar")
endif()

if(NOT "$ENV{CLICOLOR_FORCE}" STREQUAL "" AND NOT "$ENV{CLICOLOR_FORCE}" EQUAL 0)
  if(NOT MSVC)
    add_compile_options("-fdiagnostics-color=always")
  endif()
elseif(NOT "$ENV{CLICOLOR}" STREQUAL "" AND NOT "$ENV{CLICOLOR}" EQUAL 0)
  if(NOT MSVC)
    add_compile_options("-fdiagnostics-color=auto")
  endif()
elseif(NOT "$ENV{CLICOLOR}" STREQUAL "")
  if(NOT MSVC)
    add_compile_options("-fdiagnostics-color=never")
  endif()
endif()
