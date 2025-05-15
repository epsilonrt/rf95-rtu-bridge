cmake_minimum_required(VERSION 3.0)

# 
project(rf95_rtu_bridge)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# set(CMAKE_VERBOSE_MAKEFILE ON)

find_package(PkgConfig REQUIRED) 
# pkg_check_modules(MODBUSPP REQUIRED IMPORTED_TARGET modbuspp)
pkg_check_modules(RADIOHEAD REQUIRED IMPORTED_TARGET radiohead)
pkg_check_modules(PIDUINO REQUIRED IMPORTED_TARGET piduino)

file(GLOB SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/poo-toolbox/src/*.cpp)
add_executable(${PROJECT_NAME} ${SOURCES})
target_link_libraries(${PROJECT_NAME} PUBLIC PkgConfig::RADIOHEAD PkgConfig::PIDUINO)

# Inclure les fichiers d'en-tête de la bibliothèque
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/poo-toolbox/src)

set(PROGRAM_PERMISSIONS_BY_ROOT
    OWNER_WRITE OWNER_READ OWNER_EXECUTE
    GROUP_READ GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE SETUID)
install(TARGETS ${PROJECT_NAME} DESTINATION bin PERMISSIONS ${PROGRAM_PERMISSIONS_BY_ROOT})
