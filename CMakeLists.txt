cmake_minimum_required (VERSION 3.8)

if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("dynamic")

find_package(SEAL REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(ZMQ libzmq)

add_executable (dynamic "homomorphic.cpp" "main.cpp" "polynomial.cpp" "bulletin.cpp" "zmq.hpp")

target_include_directories(dynamic PRIVATE 
    "${CMAKE_CURRENT_SOURCE_DIR}/libzmq/include"
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(dynamic PRIVATE gmp SEAL::seal ${ZMQ_LIBRARIES} pthread)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET dynamic PROPERTY CXX_STANDARD 20)
endif()