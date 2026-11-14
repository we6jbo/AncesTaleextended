cmake_minimum_required(VERSION 3.21)

project(AncesTaleextended VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

qt_add_executable(AncesTaleextended
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
)

target_link_libraries(AncesTaleextended PRIVATE Qt6::Widgets)

install(TARGETS AncesTaleextended
    RUNTIME DESTINATION bin
)
