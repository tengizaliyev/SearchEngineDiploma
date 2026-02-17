cmake_minimum_required(VERSION 3.16)
project(SearchEngineDiploma)

set(CMAKE_CXX_STANDARD 17)

find_package(Boost REQUIRED COMPONENTS system)
find_package(OpenSSL REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(PQXX REQUIRED libpqxx)

include_directories(include)
include_directories(${Boost_INCLUDE_DIRS})
include_directories(${PQXX_INCLUDE_DIRS})

add_executable(spider spider/main.cpp)
target_link_libraries(spider
    ${Boost_LIBRARIES}
    OpenSSL::SSL OpenSSL::Crypto
    ${PQXX_LIBRARIES}
)

add_executable(indexer indexer/main.cpp)
target_link_libraries(indexer
    ${PQXX_LIBRARIES}
)

add_executable(searcher searcher/main.cpp)
target_link_libraries(searcher
    ${Boost_LIBRARIES}
    ${PQXX_LIBRARIES}
)
