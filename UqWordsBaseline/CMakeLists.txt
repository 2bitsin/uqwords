add_executable(baseline sources/main.cpp)
set_property(TARGET baseline PROPERTY CXX_STANDARD 20)
find_package(fmt)
target_link_libraries(baseline fmt::fmt)
