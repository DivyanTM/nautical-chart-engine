cmake_minimum_required(VERSION 3.20)

project(nautical_chart_engine
    VERSION 0.1.0
    LANGUAGES C
)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(ENABLE_SANITIZERS "Enable address and undefined behavior sanitizers" OFF)

if(ENABLE_SANITIZERS)
    add_compile_options(
        -fsanitize=address,undefined
        -fno-omit-frame-pointer
        -g
    )

    add_link_options(
        -fsanitize=address,undefined
    )
endif()

add_executable(nautical_chart_engine
   src/main.c
   src/iso8211/leader_parser.c
   src/iso8211/directory_parser.c
   src/iso8211/record_reader.c
   src/iso8211/ddr_parser.c
)

target_include_directories(nautical_chart_engine PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(nautical_chart_engine PRIVATE
      -Wall
      -Wextra
      -Wpedantic
      -Werror
  )
endif()

