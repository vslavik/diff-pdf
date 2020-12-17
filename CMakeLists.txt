project(diff-pdf)
cmake_minimum_required(VERSION 3.0)

set(CMAKE_CXX_STANDARD 11)

include(FindPkgConfig)

pkg_search_module(POPPLERGLIB REQUIRED poppler-glib)

add_executable(diff-pdf diff-pdf.cpp)
target_include_directories(diff-pdf PRIVATE
    "${POPPLERGLIB_INCLUDE_DIRS}"
)
target_link_libraries(diff-pdf ${POPPLERGLIB_LIBRARIES})
target_link_directories(diff-pdf PRIVATE
    ${POPPLERGLIB_LIBRARY_DIRS}
)
install(TARGETS diff-pdf)
