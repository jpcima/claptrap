###
include("cmake.deps.txt")

###
add_library(ct-examples-common STATIC
  "sources/common/include_vstgui.hpp"
  "sources/common/vstgui_init.cpp"
  "sources/common/vstgui_init.hpp"
  "sources/common/vstgui_clap_gui.cpp"
  "sources/common/vstgui_clap_gui.hpp"
  "sources/common/vstgui_x11_runloop.cpp"
  "sources/common/vstgui_x11_runloop.hpp"
  "sources/common/apple_bundle.cpp"
  "sources/common/apple_bundle.hpp")
target_include_directories(ct-examples-common PUBLIC "sources")
target_link_libraries(ct-examples-common PUBLIC ct-uidescription ct-vstgui ct-clap PRIVATE sane-warning-flags)

###
add_v3_plugin(gain
  NAME "Gain"
  BUNDLE_IDENTIFIER "com.example.Gain"
  COPYRIGHT "Free software under the ISC License"
  HIGH_RESOLUTION_CAPABLE TRUE
  SOURCES
  "sources/gain/gain.cpp"
  RESOURCES
  "sources/gain//gain.uidesc")
target_link_libraries(gain PRIVATE ct-examples-common sane-warning-flags)
