# CleanOldPlugins.cmake
# Removes stale versioned plugin bundles that share the same AU/VST3 plugin code
# as the current build but have a version number in their name (e.g. "Howling Wolves 3.2.component").
# Multiple bundles with the same PLUGIN_CODE conflict: CoreAudio loads whichever it finds
# first, which may be an old broken build rather than the newly deployed one.
#
# Called as a cmake -P script from CMakeLists.txt POST_BUILD steps.
# Required variables (passed via -D):
#   SEARCH_DIR   — directory to scan (e.g. ~/Library/Audio/Plug-Ins/Components)
#   PLUGIN_NAME  — base name without extension (e.g. "Howling Wolves")
#   EXT          — bundle extension without dot (e.g. component, vst3, app)

file(GLOB stale_bundles
    "${SEARCH_DIR}/${PLUGIN_NAME} [0-9].${EXT}"
    "${SEARCH_DIR}/${PLUGIN_NAME} [0-9].[0-9].${EXT}"
    "${SEARCH_DIR}/${PLUGIN_NAME} [0-9].[0-9].[0-9].${EXT}"
    "${SEARCH_DIR}/${PLUGIN_NAME} [0-9][0-9].${EXT}"
    "${SEARCH_DIR}/${PLUGIN_NAME} [0-9][0-9].[0-9].${EXT}"
    "${SEARCH_DIR}/${PLUGIN_NAME} [0-9][0-9].[0-9].[0-9].${EXT}"
)

foreach(bundle ${stale_bundles})
    message(STATUS "Wolf's Den cleanup: removing stale bundle ${bundle}")
    file(REMOVE_RECURSE "${bundle}")
endforeach()
