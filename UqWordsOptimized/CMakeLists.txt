add_executable(app1 sources/main.cpp)
set_property(TARGET app1 PROPERTY CXX_STANDARD 20)
find_package(fmt)
target_link_libraries(app1 fmt::fmt)
