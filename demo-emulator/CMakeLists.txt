add_executable(demo main.cpp)
find_package(Git)

execute_process(COMMAND git log --date=format:%Y%m%d --pretty=format:'%cd' -n 1 WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" OUTPUT_VARIABLE GIT_DATE)

execute_process(COMMAND git log --pretty=format:'%h' -n 1
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    OUTPUT_VARIABLE GIT_REV)

execute_process(
    COMMAND git diff --quiet --exit-code
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    RESULT_VARIABLE GIT_DIRTY ERROR_QUIET)

execute_process(
    COMMAND git rev-parse --abbrev-ref HEAD
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    OUTPUT_VARIABLE GIT_BRANCH)

string(SUBSTRING ${GIT_DATE} 1 8 GIT_DATE)
string(STRIP "${GIT_REV}" GIT_REV)
string(SUBSTRING "${GIT_REV}" 1 7 GIT_REV)
string(STRIP "${GIT_DIRTY}" GIT_DIRTY)
string(STRIP "${GIT_BRANCH}" GIT_BRANCH)
set(VERSION "0.1")
option(VERSION_TAG "Version tag" "draft")

configure_file(manifest.json manifest.json @ONLY)
configure_file(ui.xml ui.xml @ONLY)
configure_file(demo.svg demo.svg COPYONLY)

install(
    FILES
        ${CMAKE_BINARY_DIR}/manifest.json
        ${CMAKE_BINARY_DIR}/ui.xml
    TARGETS
        demo

    DESTINATION .
)
