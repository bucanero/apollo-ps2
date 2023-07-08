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
#  data/*.s3m
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
  source/sfo.c
  source/ttf_fonts.c
  source/util.c
  source/ps2pad.c
  source/ps2icon.c
  source/psv_ps2.c
  source/psv_resign.c
  source/zip_util.c
  source/vmp_resign.c
)

target_link_libraries(${PROJECT_NAME}
  ${SDL2_LIBRARIES}
#  SDL2main
#  SDL2
#  ps2_drivers
#  patches
#  pad
#  gskit
#  dmakit
#  curl
#  ps2ip
  CURL::libcurl
  apollo
  polarssl
  dbglogger
  freetype
  png
#  pthread
#  unrar
  zip
  lzma
#  un7zip
#  xmp
#  bz2
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

add_custom_target(copy
  COMMAND @echo "Copying to $$PSPMS/PSP/GAME/APOLLO/EBOOT.PBP ..."
  COMMAND @mkdir -p $$PSPMS/PSP/GAME/APOLLO
  COMMAND @cp -v EBOOT.PBP $$PSPMS/PSP/GAME/APOLLO/EBOOT.PBP
  DEPENDS ${PROJECT_NAME}
)

add_custom_target(send
  COMMAND @echo "Uploading to ftp://$$PSPIP:1337/ms0:/PSP/GAME/APOLLO/EBOOT.PBP ..."
  COMMAND @curl -T EBOOT.PBP ftp://$$PSPIP:1337/ms0:/PSP/GAME/APOLLO/EBOOT.PBP
  DEPENDS ${PROJECT_NAME}
)
