###############################################
# apps/skelTest
file(GLOB skelTest_srcs "*.cpp")
file(GLOB skelTest_hdrs "*.h")
add_executable(skelTest ${skelTest_srcs} ${skelTest_hdrs})
target_link_libraries(skelTest dart)
set_target_properties(skelTest PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
