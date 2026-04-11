# Deploy one built plug-in bundle: optional Factory WAVs into Resources/Factory, then full copy to two destinations.
# -DSRC=           Path to built .vst3 / .component / .app bundle (folder)
# -DDST_PRODUCTS=  e.g. .../apex_project/Products/WolfsDen.vst3
# -DDST_USER=      e.g. ~/Library/.../Wolf's Den.vst3 (may be empty to skip)
# -DFACTORY=       Path to DistributionContent (may be empty to skip)

if(NOT SRC)
    message(FATAL_ERROR "DeployWolfsDenPlugin: -DSRC= required")
endif()
if(NOT DST_PRODUCTS)
    message(FATAL_ERROR "DeployWolfsDenPlugin: -DDST_PRODUCTS= required")
endif()

if(DEFINED FACTORY AND EXISTS "${FACTORY}")
    set(_factory_dest "${SRC}/Contents/Resources/Factory")
    file(MAKE_DIRECTORY "${SRC}/Contents/Resources")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E copy_directory "${FACTORY}" "${_factory_dest}"
        RESULT_VARIABLE _r
    )
    if(_r)
        message(FATAL_ERROR "copy_directory Factory -> bundle failed: ${_r}")
    endif()
endif()

macro(_wd_copy dst)
    if(NOT "${dst}" STREQUAL "")
        if(EXISTS "${dst}")
            file(REMOVE_RECURSE "${dst}")
        endif()
        execute_process(
            COMMAND "${CMAKE_COMMAND}" -E copy_directory "${SRC}" "${dst}"
            RESULT_VARIABLE _r2
        )
        if(_r2)
            message(FATAL_ERROR "copy_directory ${SRC} -> ${dst} failed: ${_r2}")
        endif()
    endif()
endmacro()

_wd_copy("${DST_PRODUCTS}")
_wd_copy("${DST_USER}")
