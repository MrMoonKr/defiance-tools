cmake_minimum_required(VERSION 3.16)

project(defiance-tools LANGUAGES C CXX)

if(NOT WIN32)
  message(FATAL_ERROR "defiance-tools currently supports Windows builds only.")
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(WARNING "The original Visual Studio solution is Win32-only. Use '-A Win32' for parity, especially for snd2wav and mss32.dll.")
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(DEFIANCE_TOOLS_DEPLOY_DIR "${CMAKE_SOURCE_DIR}/bin")

set(ZLIB_SOURCES
  zlib/adler32.c
  zlib/compress.c
  zlib/crc32.c
  zlib/deflate.c
  zlib/gzclose.c
  zlib/gzlib.c
  zlib/gzread.c
  zlib/gzwrite.c
  zlib/infback.c
  zlib/inffast.c
  zlib/inflate.c
  zlib/inftrees.c
  zlib/trees.c
  zlib/uncompr.c
  zlib/zutil.c
)

add_library(zlib STATIC ${ZLIB_SOURCES})
target_include_directories(zlib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/zlib")
target_compile_definitions(zlib PRIVATE _CRT_NONSTDC_NO_DEPRECATE _CRT_SECURE_NO_WARNINGS _MBCS)
set_target_properties(zlib PROPERTIES FOLDER third_party)

set(LIBPNG_SOURCES
  libpng/png.c
  libpng/pngerror.c
  libpng/pnggccrd.c
  libpng/pngget.c
  libpng/pngmem.c
  libpng/pngpread.c
  libpng/pngread.c
  libpng/pngrio.c
  libpng/pngrtran.c
  libpng/pngrutil.c
  libpng/pngset.c
  libpng/pngtrans.c
  libpng/pngvcrd.c
  libpng/pngwio.c
  libpng/pngwrite.c
  libpng/pngwtran.c
  libpng/pngwutil.c
)

add_library(libpng STATIC ${LIBPNG_SOURCES})
target_include_directories(
  libpng
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/libpng"
    "${CMAKE_CURRENT_SOURCE_DIR}/zlib"
)
target_compile_definitions(libpng PRIVATE _CRT_SECURE_NO_WARNINGS _MBCS)
target_link_libraries(libpng PUBLIC zlib)
set_target_properties(libpng PROPERTIES FOLDER third_party)

set(WADLIB_SOURCES
  wadlib/dxt.c
  wadlib/mes.c
  wadlib/objfile.c
  wadlib/ski.c
  wadlib/snd.c
  wadlib/tex.c
  wadlib/util.c
  wadlib/wadlib.c
)

add_library(wadlib STATIC ${WADLIB_SOURCES})
target_include_directories(
  wadlib
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/wadlib"
    "${CMAKE_CURRENT_SOURCE_DIR}/libpng"
    "${CMAKE_CURRENT_SOURCE_DIR}/zlib"
)
target_compile_definitions(wadlib PUBLIC WIN32)
target_link_libraries(wadlib PUBLIC libpng zlib)
set_target_properties(wadlib PROPERTIES FOLDER libraries)

function(add_defiance_tool target source_file)
  add_executable(${target} "${CMAKE_CURRENT_SOURCE_DIR}/${source_file}")
  target_link_libraries(${target} PRIVATE wadlib)
  target_compile_definitions(${target} PRIVATE WIN32 _CONSOLE)
  add_custom_command(
    TARGET ${target}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${DEFIANCE_TOOLS_DEPLOY_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:${target}>"
            "${DEFIANCE_TOOLS_DEPLOY_DIR}/$<TARGET_FILE_NAME:${target}>"
  )
  set_target_properties(${target} PROPERTIES FOLDER tools)
endfunction()

add_defiance_tool(mes2obj mes2obj/mes2obj.c)
add_defiance_tool(ski2obj ski2obj/ski2obj.c)
add_defiance_tool(snd2wav snd2wav/snd2wav.c)
add_defiance_tool(tex2png tex2png/tex2png.c)
add_defiance_tool(waddump waddump/waddump.c)
add_defiance_tool(wadsdiff wadsdiff/wadsdiff.c)

add_subdirectory(wadexplorer)
