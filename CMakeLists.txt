cmake_minimum_required (VERSION 3.8)

if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("dynamic")

add_executable (dynamic 
    "dynamic.cpp" 
    "dynamic.h"
    "ecc-cpp/src/bigint.cpp"
    "ecc-cpp/src/ecc.cpp"
    "ecc-cpp/src/ecdh.cpp"
    "ecc-cpp/src/ecdsa.cpp"
    "ecc-cpp/src/blake2b.cpp"
)

target_include_directories(dynamic PRIVATE 
    "${CMAKE_CURRENT_SOURCE_DIR}/ecc-cpp/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/ecc-cpp/src"
)

target_link_libraries(dynamic gmp)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET dynamic PROPERTY CXX_STANDARD 20)
endif()