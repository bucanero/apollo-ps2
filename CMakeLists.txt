cmake_minimum_required(VERSION 3.2)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(NOT DEFINED ENV{PS2DEV})
    message(FATAL_ERROR "Please define PS2DEV to point to your SDK path!")
  endif()
endif()

project(apollo-ps2.elf)

find_package(SDL2 REQUIRED)
find_package(CURL REQUIRED)

option(APOLLO_ENABLE_LOGGING "enables debug logging over udp multicast" OFF)

if(APOLLO_ENABLE_LOGGING)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DAPOLLO_ENABLE_LOGGING")
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu11")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s")

include_directories(
  include
  $ENV{PS2SDK}/ports/include/freetype2
)

link_directories(
  libs
)

# From vita shell CMakeLists - credits TheFlow
FUNCTION(ADD_RESOURCES out_var)
    SET(result)
    FOREACH(in_f ${ARGN})
        SET(out_f "${CMAKE_CURRENT_BINARY_DIR}/${in_f}.o")
        GET_FILENAME_COMPONENT(out_dir ${out_f} DIRECTORY)
        ADD_CUSTOM_COMMAND(OUTPUT ${out_f}
                COMMAND ${CMAKE_COMMAND} -E make_directory ${out_dir}
                COMMAND ${CMAKE_LINKER} -r -b binary -o ${out_f} ${in_f}
                DEPENDS ${in_f}
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                COMMENT "Building resource ${out_f}"
                VERBATIM
                )
        LIST(APPEND result ${out_f})
    ENDFOREACH()
    SET(${out_var} "${result}" PARENT_SCOPE)
ENDFUNCTION()

file(GLOB res_files RELATIVE
  ${CMAKE_SOURCE_DIR}
  data/*.png
  data/*.irx
  data/*.ahx
)
add_resources(apollo_res ${res_files})

add_executable(${PROJECT_NAME}
  ${apollo_res}
  source/common.c
  source/config.c
  source/dialog.c
  source/draw.c
  source/exec_cmd.c
  source/http.c
  source/libfont.c
  source/lzari.c
  source/main.c
  source/menu_hex.c
  source/menu_about.c
  source/menu_cheats.c
  source/menu_options.c
  source/menu_main.c
  source/saves.c
  source/settings.c
  source/mcio.c
  source/ttf_fonts.c
  source/util.c
  source/ps1card.c
  source/ps2pad.c
  source/ps2icon.c
  source/import_ps2.c
  source/psv_resign.c
  source/zip_util.c
)

target_link_libraries(${PROJECT_NAME}
  ${SDL2_LIBRARIES}
  CURL::libcurl
  apollo
  polarssl
  dbglogger
  freetype
  png
#  unrar
  zip
  lzma
  mc
#  un7zip
  ahx
  z
)

add_custom_target(createzip
  COMMAND @mkdir -p APOLLO/DATA
  COMMAND @mkdir -p APOLLO/CACHE
  COMMAND @echo "Downloading appdata.zip ..."
  COMMAND @rm -fr apollo-psp.zip
  COMMAND @curl -L "https://bucanero.github.io/apollo-patches/PSP/apollo-psp-update.zip" > APOLLO/CACHE/appdata.zip
  COMMAND @cp EBOOT.PBP APOLLO/
  COMMAND @cp ${CMAKE_SOURCE_DIR}/data/fonts/* APOLLO/DATA/
  COMMAND @zip -r apollo-psp.zip APOLLO/
)

add_custom_target(packelf
  COMMAND @echo "Packing release ELF ..."
  COMMAND @${PS2DEV}/bin/ps2-packer-lite ${PROJECT_NAME} APOLLO.ELF
  DEPENDS ${PROJECT_NAME}
)

add_custom_target(release
  COMMAND @echo "Packing release ELF ..."
  COMMAND @${PS2DEV}/bin/ps2-packer-lite ${PROJECT_NAME} APOLLO.ELF
  COMMAND @echo "Copying to /APOLLO-99PS2/ ..."
  COMMAND @mkdir -p APOLLO-99PS2/
  COMMAND @cp -v APOLLO.ELF APOLLO-99PS2/APOLLO.ELF
  COMMAND @cp -v ${CMAKE_SOURCE_DIR}/icon.sys APOLLO-99PS2/icon.sys
  COMMAND @cp -v ${CMAKE_SOURCE_DIR}/icon.ico APOLLO-99PS2/icon.ico
  COMMAND @zip -r apollo-ps2.zip APOLLO-99PS2/
  DEPENDS ${PROJECT_NAME}
)
