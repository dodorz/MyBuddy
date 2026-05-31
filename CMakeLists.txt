cmake_minimum_required(VERSION 3.20)
project(MyBuddy LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(MyBuddy WIN32
  src/main.cpp
  src/app.cpp
  src/MyBuddy.rc
  src/notes.cpp
  src/state.cpp
  src/window.cpp
)

target_include_directories(MyBuddy PRIVATE src)
target_link_libraries(MyBuddy PRIVATE user32 gdi32 comctl32 shell32 advapi32)
if(MINGW)
  target_link_options(MyBuddy PRIVATE -municode)
endif()
if(MSVC)
  target_link_options(MyBuddy PRIVATE
    "/MANIFEST:EMBED"
    "/MANIFESTINPUT:${CMAKE_CURRENT_SOURCE_DIR}/src/MyBuddy.manifest")
endif()

target_compile_definitions(MyBuddy PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
