add_executable(pjson_smoke test_smoke.cpp)
target_link_libraries(pjson_smoke PRIVATE pjson::pjson)
add_test(NAME pjson.smoke COMMAND pjson_smoke)

add_executable(pjson_node_layout test_node_layout.cpp)
target_link_libraries(pjson_node_layout PRIVATE pjson::pjson)
add_test(NAME pjson.node_layout COMMAND pjson_node_layout)

add_executable(pjson_scalar test_scalar.cpp)
target_link_libraries(pjson_scalar PRIVATE pjson::pjson)
add_test(NAME pjson.scalar COMMAND pjson_scalar)

add_executable(pjson_string_binary test_string_binary.cpp)
target_link_libraries(pjson_string_binary PRIVATE pjson::pjson)
add_test(NAME pjson.string_binary COMMAND pjson_string_binary)
