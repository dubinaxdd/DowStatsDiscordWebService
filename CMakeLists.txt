cmake_minimum_required(VERSION 3.22)

project(DowStatsDiscordWebService LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Network WebSockets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Network WebSockets)

add_executable(DowStatsDiscordWebService
  main.cpp
  defines.h
  DiscordWebProcessor.h
  DiscordWebProcessor.cpp
  NewsServer.h
  NewsServer.cpp
  BaseTypes.h
)


set( OPENSSL_ROOT_DIR ${OPENSSL_ROOT_DIR})
#set( OPENSSL_ROOT_DIR "D:/Qt/6.9.0/Tools/OpenSSLv3/Win_x64")

find_package(OpenSSL REQUIRED)

target_link_libraries(DowStatsDiscordWebService
  Qt${QT_VERSION_MAJOR}::Core
  Qt${QT_VERSION_MAJOR}::Network
  Qt${QT_VERSION_MAJOR}::WebSockets

  OpenSSL::SSL
  OpenSSL::Crypto
)

include(GNUInstallDirs)
install(TARGETS DowStatsDiscordWebService
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
