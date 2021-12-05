add_executable(optimized0 sources/main.cpp)
set_property(TARGET optimized0 PROPERTY CXX_STANDARD 20)
find_package(fmt)
target_link_libraries(optimized0 fmt::fmt)
