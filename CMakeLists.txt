cmake_minimum_required(VERSION 3.18)
project(claptrap LANGUAGES C CXX VERSION 0.0.0)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

###
set(CT_ROOT_DIR "${PROJECT_SOURCE_DIR}" CACHE
  FILEPATH "Root directory of the ${CMAKE_PROJECT_NAME} project."
  FORCE)

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
option(CT_ASSERTIONS "Enable assertions regardless of build type" OFF)
option(CT_DOWNLOAD_CLAP "Download the CLAP library" OFF)
set(CT_CLAP_INCLUDE_DIR "" CACHE FILEPATH "Path to CLAP headers (optional)")

###
include("cmake.deps.txt")
include("cmake.v3.txt")

###
add_library(ct-v3 OBJECT
  "sources/v3/ct_wrapper.cpp"
  "sources/v3/ct_defs.cpp"
  "sources/v3/ct_defs.hpp"
  "sources/v3/ct_component.cpp"
  "sources/v3/ct_component.hpp"
  "sources/v3/ct_component_caches.cpp"
  "sources/v3/ct_component_caches.hpp"
  "sources/v3/ct_audio_processor.cpp"
  "sources/v3/ct_audio_processor.hpp"
  "sources/v3/ct_edit_controller.cpp"
  "sources/v3/ct_edit_controller.hpp"
  "sources/v3/ct_process_context_requirements.cpp"
  "sources/v3/ct_process_context_requirements.hpp"
  "sources/v3/ct_plug_view.cpp"
  "sources/v3/ct_plug_view.hpp"
  "sources/v3/ct_plug_view_content_scale.cpp"
  "sources/v3/ct_plug_view_content_scale.hpp"
  "sources/v3/ct_channel_mapping.cpp"
  "sources/v3/ct_channel_mapping.hpp"
  "sources/v3/ct_event_conversion.cpp"
  "sources/v3/ct_event_conversion.hpp"
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
  "sources/v3/ct_host_loop.cpp"
  "sources/v3/ct_host_loop.hpp"
  "sources/v3/ct_host_loop_posix.cpp"
  "sources/v3/ct_host_loop_posix.hpp"
  "sources/v3/ct_plugin_factory.cpp"
  "sources/v3/ct_plugin_factory.hpp"
  "sources/v3/ct_stream.cpp"
  "sources/v3/ct_stream.hpp"
  "sources/v3/ct_threads.cpp"
  "sources/v3/ct_threads.hpp"
  "sources/v3/clap_helpers.cpp"
  "sources/v3/clap_helpers.hpp"
  "sources/v3/travesty_helpers.hpp"
  "sources/utility/ct_assert.hpp"
  "sources/utility/ct_attributes.hpp"
  "sources/utility/ct_bump_allocator.cpp"
  "sources/utility/ct_bump_allocator.hpp"
  "sources/utility/ct_memory.hpp"
  "sources/utility/ct_messages.hpp"
  "sources/utility/ct_posix_fd.hpp"
  "sources/utility/ct_posix_pipe.cpp"
  "sources/utility/ct_posix_pipe.hpp"
  "sources/utility/ct_uuid.cpp"
  "sources/utility/ct_uuid.hpp"
  "sources/utility/ct_safe_fnptr.hpp"
  "sources/utility/ct_scope.hpp"
  "sources/utility/unicode_helpers.hpp"
  "sources/utility/url_helpers.cpp"
  "sources/utility/url_helpers.hpp"
  "sources/libs/heapsort.hpp"
  "sources/libs/span.hpp"
  "sources/libs/utf.hpp")
target_compile_definitions(ct-v3 PUBLIC "CLAP_EXPORT=")
target_include_directories(ct-v3 PRIVATE "sources")
target_link_libraries(ct-v3 PUBLIC ct-clap PRIVATE ct-travesty sane-warning-flags Threads::Threads)

if(CT_ASSERTIONS)
  target_compile_definitions(ct-v3 PRIVATE "CT_ASSERTIONS")
endif()

###
if(CT_EXAMPLES)
  add_subdirectory("examples")
endif()
