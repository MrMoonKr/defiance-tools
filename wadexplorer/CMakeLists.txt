set(WADEXPLORER_SOURCES
  main.cpp
  Mainfrm.cpp
  WadExplorerApp.cpp
  ExplorerView.cpp
  GLPage.cpp
  ImagePage.cpp
  TextPage.cpp
  WadData.cpp
)

add_executable(wadexplorer WIN32 ${WADEXPLORER_SOURCES})

target_include_directories(
  wadexplorer
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/third_party/Win32xx/include"
    "${CMAKE_SOURCE_DIR}/wadlib"
)

target_link_libraries(
  wadexplorer
  PRIVATE
    wadlib
    comctl32
    comdlg32
    shell32
    shlwapi
    ole32
    opengl32
)

target_compile_definitions(
  wadexplorer
  PRIVATE
    WIN32
    UNICODE
    _UNICODE
    NOMINMAX
    WADEXPLORER_PROJECT_DIR="${CMAKE_SOURCE_DIR}"
)

if(MSVC)
  target_compile_options(wadexplorer PRIVATE /EHsc /W4)
else()
  target_compile_options(wadexplorer PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_custom_command(
  TARGET wadexplorer
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${DEFIANCE_TOOLS_DEPLOY_DIR}"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "$<TARGET_FILE:wadexplorer>"
          "${DEFIANCE_TOOLS_DEPLOY_DIR}/$<TARGET_FILE_NAME:wadexplorer>"
)

set_target_properties(wadexplorer PROPERTIES FOLDER tools)
