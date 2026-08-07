add_executable(pjson_smoke test_smoke.cpp)
target_link_libraries(pjson_smoke PRIVATE pjson::pjson)

add_test(NAME pjson.smoke COMMAND pjson_smoke)
