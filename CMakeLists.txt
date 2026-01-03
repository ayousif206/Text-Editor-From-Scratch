cmake_minimum_required(VERSION 4.0)
project(Text_Editor_From_Scratch)

set(CMAKE_CXX_STANDARD 20)

add_executable(Text_Editor_From_Scratch
        main.cpp
        Data.cpp
        Data.h
        Terminal.cpp
        Terminal.h
        Row.cpp
        Row.h
        output.cpp
        output.h
        fileIO.cpp
        fileIO.h
        Input.cpp
        Input.h)
