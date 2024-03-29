cmake_minimum_required(VERSION 3.20)
project(ELP-Launcher)

set(QT_MIN_VER 6.2.0)

find_package(Boost 1.83 REQUIRED COMPONENTS system)

find_package(Qt6 ${QT_MIN_VER} CONFIG COMPONENTS Widgets Concurrent Multimedia MultimediaWidgets Svg SvgWidgets Xml)
add_library(qt INTERFACE)
target_link_libraries(qt INTERFACE Qt6::Widgets Qt6::Concurrent Qt6::Multimedia Qt6::MultimediaWidgets Qt6::Svg Qt6::SvgWidgets Qt6::Xml)

if(NOT WIN32)
	find_package(Qt6 ${QT_MIN_VER} COMPONENTS DBus Gui)
	target_include_directories(qt INTERFACE ${Qt6Gui_PRIVATE_INCLUDE_DIRS})

	if(Qt6DBus_FOUND)
		target_link_libraries(qt INTERFACE Qt6::DBus)
		target_compile_definitions(qt INTERFACE -DHAVE_QTDBUS)
	endif()
endif()

set(CMAKE_CXX_STANDARD 23)

add_executable(${PROJECT_NAME}
    src/AlternativeGroup.cpp
    src/AlternativeStorage.cpp
    src/Context.cpp
    src/main.cpp
    src/Widget.cpp
    src/Server.cpp
    src/NativeLauncher.cpp
    src/Protocol.cpp
    src/Transport.cpp
    src/FlowLayout.cpp
    src/Url.cpp
    src/Manifest.cpp
    src/UiFile.cpp
)

target_link_libraries(${PROJECT_NAME} PUBLIC qt Boost::system)

set_target_properties(${PROJECT_NAME}
    PROPERTIES
        AUTOMOC ON
)

add_subdirectory(dependencies/minizip-ng)
add_subdirectory(demo-emulator)
add_subdirectory(demo-repository)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/icons DESTINATION ${CMAKE_CURRENT_BINARY_DIR})


set(MZ_DECOMPRESS_ONLY on)
target_link_libraries(${PROJECT_NAME} PUBLIC qt Boost::system minizip)
