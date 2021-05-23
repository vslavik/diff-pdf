project(diff-pdf)
cmake_minimum_required(VERSION 3.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(FindPkgConfig)

pkg_search_module(POPPLERGLIB REQUIRED poppler-glib)
pkg_search_module(STDCFS REQUIRED stdc++fs)

add_executable(diff-pdf diff-pdf.cpp)
target_include_directories(diff-pdf PRIVATE
    "${POPPLERGLIB_INCLUDE_DIRS}"
    "${STDCFS_INCLUDE_DIRS}"
)

target_link_libraries(diff-pdf ${POPPLERGLIB_LIBRARIES} ${STDCFS_LIBRARIES})
target_link_directories(diff-pdf PRIVATE
    ${POPPLERGLIB_LIBRARY_DIRS} ${STDCFS_LIBRARY_DIRS}
)
install(TARGETS diff-pdf RUNTIME DESTINATION bin)
