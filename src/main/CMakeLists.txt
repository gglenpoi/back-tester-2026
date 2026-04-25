set(TARGET back-tester-main)

file(GLOB SRC CONFIGURE_DEPENDS *.cpp)

add_executable(${TARGET} ${SRC})
add_dependencies(${TARGET} GenerateVersion)

target_link_libraries(${TARGET} PRIVATE back-tester-common)