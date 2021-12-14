add_executable(app0 sources/main.cpp)
set_property(TARGET app0 PROPERTY CXX_STANDARD 20)
find_package(fmt)
target_link_libraries(app0 fmt::fmt)
