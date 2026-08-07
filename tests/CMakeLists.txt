add_executable(pjson_smoke test_smoke.cpp)
target_link_libraries(pjson_smoke PRIVATE pjson::pjson)
add_test(NAME pjson.smoke COMMAND pjson_smoke)

add_executable(pjson_node_layout test_node_layout.cpp)
target_link_libraries(pjson_node_layout PRIVATE pjson::pjson)
add_test(NAME pjson.node_layout COMMAND pjson_node_layout)
