set(TARGET lob-app)

file(GLOB SRC CONFIGURE_DEPENDS *.cpp)

add_executable(${TARGET} ${SRC})

target_link_libraries(${TARGET} PRIVATE
    back-tester-parser
    back-tester-common
    pthread
)