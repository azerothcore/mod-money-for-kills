CU_SET_PATH("CMAKE_MOD_MONEYFORKILLS_DIR" "${CMAKE_CURRENT_LIST_DIR}")

AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/mod_moneyforkills.cpp")

AC_ADD_SCRIPT_LOADER("MoneyForKills" "${CMAKE_CURRENT_LIST_DIR}/src/loader_moneyforkills.h")

AC_ADD_CONFIG_FILE("${CMAKE_CURRENT_LIST_DIR}/conf/mod_moneyforkills.conf.dist")
