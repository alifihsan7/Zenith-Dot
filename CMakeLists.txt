cmake_minimum_required(VERSION 3.21)
project(ZenithDot VERSION 0.1.0 LANGUAGES CXX RC)

if(NOT WIN32 OR NOT MSVC)
    message(FATAL_ERROR "Zenith Dot requires Windows, MSVC, and the Windows SDK.")
endif()

add_executable(ZenithDot WIN32
    src/main.cpp
    src/island.cpp
    src/media.cpp
    src/lyrics.cpp
    src/power.cpp
    src/system.cpp
    src/desktop.cpp
    src/bluetooth.cpp
    src/scratchpad.cpp
    src/settings.cpp
    src/settings_window.cpp
    src/drop_zone.cpp
    src/browser_tabs.cpp
    src/zenith.rc
)
target_compile_features(ZenithDot PRIVATE cxx_std_20)
target_compile_definitions(ZenithDot PRIVATE
    UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX
    WINVER=0x0A00 _WIN32_WINNT=0x0A00
)
target_include_directories(ZenithDot PRIVATE src)
target_compile_options(ZenithDot PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:/W4;/permissive-;/utf-8;/EHsc>")
target_link_options(ZenithDot PRIVATE /MANIFEST:NO)
set_property(TARGET ZenithDot PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
target_link_libraries(ZenithDot PRIVATE d2d1 dwrite gdi32 user32 shell32 ole32 oleaut32 windowsapp advapi32 dwmapi wbemuuid comctl32 version)
target_link_libraries(ZenithDot PRIVATE mfplat mfsensorgroup windowscodecs)
install(TARGETS ZenithDot RUNTIME DESTINATION .)
install(FILES scripts/install.ps1 scripts/uninstall.ps1 DESTINATION .)
install(FILES scripts/enable-browser-tabs.ps1 DESTINATION .)
install(DIRECTORY browser DESTINATION .)
