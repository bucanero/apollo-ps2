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
# Every gettimeofday() caller (libcurl's timeouts) gets the fixed one in source/ps2time.c
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--wrap=gettimeofday")
# lwIP socket calls set errno (see source/ps2socket.c)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--wrap=lwip_connect,--wrap=lwip_recv,--wrap=lwip_recvfrom,--wrap=lwip_send,--wrap=lwip_sendto")
# Cache files in RAM when there's no USB storage (see source/ramcache.c)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--wrap=fopen,--wrap=stat,--wrap=remove,--wrap=unlink,--wrap=chmod,--wrap=mkdir")

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

# Network IOP modules (DEV9, NETMAN, SMAP), embedded from the ps2sdk build.
# They're linked from the build folder so the symbols are _binary_<name>_irx_*
set(NET_IRX_OBJS)
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

  set(PATCHES_STAGE ${CMAKE_CURRENT_BINARY_DIR}/patches)
  add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/patches_zip.o
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${PATCHES_STAGE}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PATCHES_STAGE}
    COMMAND ${CMAKE_COMMAND} -E copy ${patch_files} ${PATCHES_STAGE}
    COMMAND ${CMAKE_COMMAND} -E chdir ${PATCHES_STAGE} ${CMAKE_COMMAND} -E tar cf ${CMAKE_CURRENT_BINARY_DIR}/patches.zip --format=zip ${patch_names}
    COMMAND ${CMAKE_LINKER} -r -b binary -o patches_zip.o patches.zip
    COMMAND ${CMAKE_OBJCOPY} --set-section-alignment .data=64 patches_zip.o
    DEPENDS ${patch_files}
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
  source/ps2socket.c
  source/ramcache.c
  source/ps2time.c
  source/libfont.c
  source/lzari.c
  source/main.c
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
