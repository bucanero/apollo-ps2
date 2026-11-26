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
# Every gettimeofday() caller (libcurl's timeouts) gets the fixed one in source/ps2_compat.c
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--wrap=gettimeofday")
# lwIP socket calls set errno (see source/ps2_compat.c)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--wrap=lwip_connect,--wrap=lwip_recv,--wrap=lwip_recvfrom,--wrap=lwip_send,--wrap=lwip_sendto")
# Cache files in RAM when there's no USB storage (see source/ramcache.c)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--wrap=fopen,--wrap=stat,--wrap=remove,--wrap=unlink,--wrap=chmod,--wrap=mkdir,--wrap=open,--wrap=read,--wrap=lseek,--wrap=close")

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
                # ld -b binary gives no alignment guarantee, but embedded IRX
                # modules are DMA'd to the IOP (SifExecModuleBuffer) and fail to
                # load (or load garbage) from a misaligned address
                COMMAND ${CMAKE_OBJCOPY} --set-section-alignment .data=64 ${out_f}
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

# IOP module for raw memory card access (full card dump/restore), built from source
set(MCIMAGE_DIR ${CMAKE_SOURCE_DIR}/iop/mcimage)
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/mcimage_irx.o
  COMMAND make -C ${MCIMAGE_DIR} IOP_OBJS_DIR=${CMAKE_CURRENT_BINARY_DIR}/mcimage/ IOP_BIN=${CMAKE_CURRENT_BINARY_DIR}/mcimage.irx
  COMMAND ${CMAKE_LINKER} -r -b binary -o mcimage_irx.o mcimage.irx
  COMMAND ${CMAKE_OBJCOPY} --set-section-alignment .data=64 mcimage_irx.o
  DEPENDS ${MCIMAGE_DIR}/mcimage.c ${MCIMAGE_DIR}/imports.lst ${MCIMAGE_DIR}/irx_imports.h ${MCIMAGE_DIR}/Makefile include/mcimage_rpc.h
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMENT "Building IOP module mcimage.irx"
  VERBATIM
)

# MX4SIO SD card driver: iop/mx4sio_bd.irx is ps2sdk's iop/sio/mx4sio_bd at
# commit edf475f4 (2023-02-02), unchanged, built with this ps2dev image:
#   https://github.com/ps2dev/ps2sdk/tree/edf475f45af9b80377a62a4797936d20efca6646/iop/sio/mx4sio_bd
# The mx4sio_bd.irx in this image's ps2sdk is older (2022-08), and SanDisk SD
# cards don't answer it; edf475f4 has the SanDisk fixes (5217db87, edf475f4).
# Newer versions use the 64-bit sector bdm interface (41b0d638) and the newer
# sio2man hooks, which this image's bdm.irx and sio2man don't support.
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/mx4sio_bd_irx.o
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/iop/mx4sio_bd.irx mx4sio_bd.irx
  COMMAND ${CMAKE_LINKER} -r -b binary -o mx4sio_bd_irx.o mx4sio_bd.irx
  COMMAND ${CMAKE_OBJCOPY} --set-section-alignment .data=64 mx4sio_bd_irx.o
  DEPENDS ${CMAKE_SOURCE_DIR}/iop/mx4sio_bd.irx
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMENT "Embedding IOP module mx4sio_bd.irx"
  VERBATIM
)

# Network IOP modules (DEV9, NETMAN, SMAP), embedded from the ps2sdk build.
# They're linked from the build folder so the symbols are _binary_<name>_irx_*
set(NET_IRX_OBJS ${CMAKE_CURRENT_BINARY_DIR}/mx4sio_bd_irx.o)
foreach(irx ps2dev9 netman smap)
  add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${irx}_irx.o
    COMMAND ${CMAKE_COMMAND} -E copy $ENV{PS2SDK}/iop/irx/${irx}.irx ${irx}.irx
    COMMAND ${CMAKE_LINKER} -r -b binary -o ${irx}_irx.o ${irx}.irx
    COMMAND ${CMAKE_OBJCOPY} --set-section-alignment .data=64 ${irx}_irx.o
    DEPENDS $ENV{PS2SDK}/iop/irx/${irx}.irx
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Embedding IOP module ${irx}.irx"
    VERBATIM
  )
  list(APPEND NET_IRX_OBJS ${CMAKE_CURRENT_BINARY_DIR}/${irx}_irx.o)
endforeach()

# Optional: PS1 + PS2 save patches embedded in the ELF, used when a patch isn't
# in APOLLO/DATA/ (USB) or on an Apollo disc. Point APOLLO_PATCHES_DIR at an
# apollo-patches checkout; the files go in the zip flat, like the disc's PATCHES/.
set(APOLLO_PATCHES_DIR "" CACHE PATH "apollo-patches checkout to embed PS1/PS2 .savepatch files from")
set(PATCHES_OBJ)
if(APOLLO_PATCHES_DIR)
  file(GLOB patch_files ${APOLLO_PATCHES_DIR}/PS1/*.savepatch ${APOLLO_PATCHES_DIR}/PS2/*.savepatch)
  if(NOT patch_files)
    message(FATAL_ERROR "No PS1/PS2 .savepatch files found in ${APOLLO_PATCHES_DIR}")
  endif()

  set(patch_names)
  foreach(f ${patch_files})
    get_filename_component(n ${f} NAME)
    list(APPEND patch_names ${n})
  endforeach()
  set(unique_names ${patch_names})
  list(REMOVE_DUPLICATES unique_names)
  list(LENGTH patch_names n_all)
  list(LENGTH unique_names n_unique)
  if(NOT n_all EQUAL n_unique)
    message(FATAL_ERROR "A title ID has a .savepatch in both PS1/ and PS2/ of ${APOLLO_PATCHES_DIR}")
  endif()
  message(STATUS "Embedding ${n_all} save patches from ${APOLLO_PATCHES_DIR}")

  # The Python modules those patches' [python:] codes import (and the modules
  # these import in turn), from apollo-patches/python/, go in the zip's python/
  # folder. Most of python/ is for PS3/PS4 games, so it isn't embedded whole.
  set(py_files)
  set(py_names)
  set(py_scan ${patch_files})
  while(py_scan)
    set(py_next)
    foreach(f ${py_scan})
      file(STRINGS ${f} py_lines REGEX "^[ \t]*(import|from)[ \t]+[A-Za-z_]")
      foreach(l ${py_lines})
        # "import a as b, c" / "from a import b": the module names only
        string(REGEX REPLACE "^[ \t]*from[ \t]+([A-Za-z0-9_]+).*$" "\\1" l "${l}")
        string(REGEX REPLACE "^[ \t]*import[ \t]+" "" l "${l}")
        string(REGEX REPLACE "[ \t]+as[ \t]+[A-Za-z0-9_]+" "" l "${l}")
        string(REGEX REPLACE "[ \t#].*$" "" l "${l}")
        string(REPLACE "," ";" mods "${l}")
        foreach(m ${mods})
          set(py ${APOLLO_PATCHES_DIR}/python/${m}.py)
          list(FIND py_names "${m}" seen)
          if(EXISTS ${py} AND seen EQUAL -1)
            list(APPEND py_names ${m})
            list(APPEND py_files ${py})
            list(APPEND py_next ${py})
          endif()
        endforeach()
      endforeach()
    endforeach()
    set(py_scan ${py_next})
  endwhile()
  set(py_zip_names)
  foreach(m ${py_names})
    list(APPEND py_zip_names python/${m}.py)
  endforeach()
  list(LENGTH py_names n_py)
  message(STATUS "Embedding ${n_py} Python modules: ${py_names}")

  set(PATCHES_STAGE ${CMAKE_CURRENT_BINARY_DIR}/patches)
  add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/patches_zip.o
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${PATCHES_STAGE}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PATCHES_STAGE}/python
    COMMAND ${CMAKE_COMMAND} -E copy ${patch_files} ${PATCHES_STAGE}
    COMMAND ${CMAKE_COMMAND} -E copy ${py_files} ${PATCHES_STAGE}/python
    COMMAND ${CMAKE_COMMAND} -E chdir ${PATCHES_STAGE} ${CMAKE_COMMAND} -E tar cf ${CMAKE_CURRENT_BINARY_DIR}/patches.zip --format=zip ${patch_names} ${py_zip_names}
    COMMAND ${CMAKE_LINKER} -r -b binary -o patches_zip.o patches.zip
    COMMAND ${CMAKE_OBJCOPY} --set-section-alignment .data=64 patches_zip.o
    DEPENDS ${patch_files} ${py_files}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Embedding save patches (patches.zip)"
    VERBATIM
  )
  set(PATCHES_OBJ ${CMAKE_CURRENT_BINARY_DIR}/patches_zip.o)
  add_definitions(-DAPOLLO_EMBEDDED_PATCHES)
endif()

add_executable(${PROJECT_NAME}
  ${PATCHES_OBJ}
  ${apollo_res}
  ${NET_IRX_OBJS}
  ${CMAKE_CURRENT_BINARY_DIR}/mcimage_irx.o
  source/common.c
  source/config.c
  source/dialog.c
  source/draw.c
  source/exec_cmd.c
  source/http.c
  source/ramcache.c
  source/ps2_compat.c
  source/libfont.c
  source/lzari.c
  source/main.c
  source/mx4sio.c
  source/menu_hex.c
  source/menu_about.c
  source/menu_cheats.c
  source/menu_options.c
  source/menu_main.c
  source/saves.c
  source/music.c
  source/ahx_player.c
  source/settings.c
  source/mcio.c
  source/ttf_fonts.c
  source/util.c
  source/ps1card.c
  source/ps2pad.c
  source/ps2icon.c
  source/ps2render.c
  source/import_ps2.c
  source/import_ps1.c
  source/mc_image.c
  source/psv_resign.c
  source/sjis.c
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
  zip
  lzma
  mc
  elf-loader
#  un7zip
  z
  socket
  ps2ip
  netman
)

add_custom_target(packelf
  COMMAND @echo "Packing release ELF ..."
  COMMAND $ENV{PS2DEV}/bin/ps2-packer-lite ${PROJECT_NAME} APOLLO.ELF
  DEPENDS ${PROJECT_NAME}
)

# Release package: apollo-ps2.zip with the APOLLO-99PS2/ memory card folder
add_custom_target(release
  COMMAND ${CMAKE_COMMAND} -E echo "Creating apollo-ps2.zip ..."
  COMMAND ${CMAKE_COMMAND} -E remove_directory APOLLO-99PS2
  COMMAND ${CMAKE_COMMAND} -E make_directory APOLLO-99PS2
  COMMAND ${CMAKE_COMMAND} -E copy APOLLO.ELF APOLLO-99PS2/APOLLO.ELF
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/icon.sys ${CMAKE_SOURCE_DIR}/icon.ico APOLLO-99PS2/
  COMMAND ${CMAKE_COMMAND} -E remove -f apollo-ps2.zip
  COMMAND ${CMAKE_COMMAND} -E tar cf apollo-ps2.zip --format=zip APOLLO-99PS2
  DEPENDS packelf
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
