cmake_minimum_required(VERSION 3.21)
project(Zenula VERSION 0.1.0 LANGUAGES CXX RC)

if(NOT WIN32 OR NOT MSVC)
    message(FATAL_ERROR "Zenula requires Windows, MSVC, and the Windows SDK.")
endif()

add_executable(Zenula WIN32
    src/main.cpp
    src/island.cpp
    src/media.cpp
    src/audio_spectrum.cpp
    src/lyrics.cpp
    src/prayer.cpp
    src/prayer_picker.cpp
    src/notifications.cpp
    src/power.cpp
    src/system.cpp
    src/desktop.cpp
    src/bluetooth.cpp
    src/calendar.cpp
    src/scratchpad.cpp
    src/settings.cpp
    src/settings_window.cpp
    src/settings_example.cpp
    src/drop_zone.cpp
    src/browser_tabs.cpp
    src/zenith.rc
)
target_compile_features(Zenula PRIVATE cxx_std_20)
target_compile_definitions(Zenula PRIVATE
    UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX
    WINVER=0x0A00 _WIN32_WINNT=0x0A00
)
target_include_directories(Zenula PRIVATE src)
target_compile_options(Zenula PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:/W4;/permissive-;/utf-8;/EHsc>")
target_link_options(Zenula PRIVATE /MANIFEST:NO)
set_property(TARGET Zenula PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
target_link_libraries(Zenula PRIVATE d2d1 dwrite gdi32 user32 shell32 ole32 oleaut32 windowsapp advapi32 dwmapi comctl32 version)
target_link_libraries(Zenula PRIVATE mfplat mfsensorgroup windowscodecs comdlg32 mmdevapi pdh)
install(TARGETS Zenula RUNTIME DESTINATION .)
install(FILES scripts/install.ps1 scripts/uninstall.ps1 DESTINATION .)
install(FILES scripts/enable-browser-tabs.ps1 DESTINATION .)
install(DIRECTORY browser DESTINATION .)
