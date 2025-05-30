add_library (stream_copy STATIC bidirectional_stream_copy.cc)
add_library(stream_sanitized EXCLUDE_FROM_ALL STATIC bidirectional_stream_copy.cc)
target_compile_options(stream_sanitized PUBLIC ${SANITIZING_FLAGS})

set(CMAKE_CXX_STANDARD 20)

macro(add_app exec_name)
  add_executable("${exec_name}" "${exec_name}.cc")
  target_link_libraries("${exec_name}" stream_copy)
  target_link_libraries("${exec_name}" minnow_debug)
  target_link_libraries("${exec_name}" util_debug)

  add_executable("${exec_name}_sanitized" "${exec_name}.cc")
  target_compile_options("${exec_name}_sanitized" PUBLIC ${SANITIZING_FLAGS})
  target_link_options("${exec_name}_sanitized" PUBLIC ${SANITIZING_FLAGS})
  target_link_libraries("${exec_name}_sanitized" stream_sanitized)
  target_link_libraries("${exec_name}_sanitized" minnow_sanitized)
  target_link_libraries("${exec_name}_sanitized" util_sanitized)
endmacro(add_app)

add_app(webget)
add_app(tcp_native)
add_app(tcp_ipv4)
add_app(endtoend)
