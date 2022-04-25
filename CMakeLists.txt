cmake_minimum_required(VERSION 3.14)
project(claptrap LANGUAGES C CXX VERSION 0.0.0)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

###
set(CT_BUILD_FROM_HERE FALSE)
if(PROJECT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  set(CT_BUILD_FROM_HERE TRUE)
endif()

###
if(CT_BUILD_FROM_HERE)
  include(BuildType)
endif()

###
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_VISIBILITY_PRESET "hidden")
set(CMAKE_CXX_VISIBILITY_PRESET "hidden")
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
include(SaneCompilerOptions)

###
option(CT_EXAMPLES "Build the examples" "${CT_BUILD_FROM_HERE}")
option(CT_LIVE_EDITING "Enable live editing with VSTGUI" OFF)
set(CT_CLAP_SOURCE_DIR "" CACHE FILEPATH "Path to external CLAP source directory (optional)")

###
include("cmake.deps.txt")
include("cmake.v3.txt")

###
add_library(ct-v3 OBJECT
  "sources/v3/ct_wrapper.cpp"
  "sources/v3/ct_component.cpp"
  "sources/v3/ct_component.hpp"
  "sources/v3/ct_audio_processor.cpp"
  "sources/v3/ct_audio_processor.hpp"
  "sources/v3/ct_edit_controller.cpp"
  "sources/v3/ct_edit_controller.hpp"
  "sources/v3/ct_plug_view.cpp"
  "sources/v3/ct_plug_view.hpp"
  "sources/v3/ct_plug_view_content_scale.cpp"
  "sources/v3/ct_plug_view_content_scale.hpp"
  "sources/v3/ct_event_handler.cpp"
  "sources/v3/ct_event_handler.hpp"
  "sources/v3/ct_timer_handler.cpp"
  "sources/v3/ct_timer_handler.hpp"
  "sources/v3/ct_unit_description.cpp"
  "sources/v3/ct_unit_description.hpp"
  "sources/v3/ct_events.cpp"
  "sources/v3/ct_events.hpp"
  "sources/v3/ct_host.cpp"
  "sources/v3/ct_host.hpp"
  "sources/v3/ct_plugin_factory.cpp"
  "sources/v3/ct_plugin_factory.hpp"
  "sources/v3/ct_stream.cpp"
  "sources/v3/ct_stream.hpp"
  "sources/v3/clap_helpers.cpp"
  "sources/v3/clap_helpers.hpp"
  "sources/v3/travesty_helpers.hpp"
  "sources/v3/unicode_helpers.hpp"
  "sources/v3/url_helpers.cpp"
  "sources/v3/unicode_helpers.hpp")
target_compile_definitions(ct-v3 PUBLIC "CLAP_EXPORT=")
target_include_directories(ct-v3 PRIVATE "sources")
target_link_libraries(ct-v3 PUBLIC ct-clap PRIVATE ct-travesty ct-uuid ct-utf8 ct-scope-lite ct-string-view-lite ct-span-lite sane-warning-flags)

###
if(CT_EXAMPLES)
  add_library(ct-examples-common STATIC
    "examples/common/include_vstgui.hpp"
    "examples/common/vstgui_x11_runloop.cpp"
    "examples/common/vstgui_x11_runloop.hpp"
    "examples/common/apple_bundle.cpp"
    "examples/common/apple_bundle.hpp")
  target_include_directories(ct-examples-common PUBLIC "examples")
  target_link_libraries(ct-examples-common PUBLIC ct-vstgui ct-clap PRIVATE sane-warning-flags)

  add_v3_plugin(gain
    NAME "Gain"
    BUNDLE_IDENTIFIER "com.example.Gain"
    COPYRIGHT "Free software under the ISC License"
    HIGH_RESOLUTION_CAPABLE TRUE
    SOURCES
      "examples/gain/gain.cpp"
    RESOURCES
      "examples/gain//gain.uidesc")
  target_link_libraries(gain PRIVATE ct-uidescription ct-examples-common sane-warning-flags)
endif()
