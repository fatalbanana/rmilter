SET(LCTRIESRC            btrie.c
                         mempool.c)
ADD_LIBRARY(lcbtrie STATIC ${LCTRIESRC})

SET(LCTRIE_CFLAGS "-DBUILD_RSPAMD")
if ("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang" OR "${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")
    SET(LCTRIE_CFLAGS "${LCTRIE_CFLAGS} -O3")
endif ()
set_target_properties(lcbtrie PROPERTIES COMPILE_FLAGS "${LCTRIE_CFLAGS}")