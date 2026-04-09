# Usage: cmake -DSRC=<bundle> -DDST=<folder> -P CopyProductBundle.cmake
if(NOT SRC)
    message(FATAL_ERROR "CopyProductBundle: -DSRC= required")
endif()
if(NOT DST)
    message(FATAL_ERROR "CopyProductBundle: -DDST= required")
endif()
if(EXISTS "${DST}")
    file(REMOVE_RECURSE "${DST}")
endif()
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${SRC}" "${DST}"
    RESULT_VARIABLE _r
)
if(_r)
    message(FATAL_ERROR "copy_directory failed: ${_r}")
endif()
