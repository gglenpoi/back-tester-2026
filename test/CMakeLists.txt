include(CTest)

if (EXISTS ${CMAKE_SOURCE_DIR}/3rdparty/Catch2/extras/Catch.cmake)
    include(${CMAKE_SOURCE_DIR}/3rdparty/Catch2/extras/Catch.cmake)
else()
    function(catch_discover_tests TARGET)
    endfunction()
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/test)

file(GLOB TEST_SRC CONFIGURE_DEPENDS *.cpp)

set(TARGET back-tester-tests)
add_executable(${TARGET} ${TEST_SRC})

# Добавляем исходники LOB для тестов
target_sources(${TARGET} PRIVATE
    ${CMAKE_SOURCE_DIR}/src/lob/LimitOrderBook.cpp
    ${CMAKE_SOURCE_DIR}/src/lob/LobManager.cpp
)

target_include_directories(${TARGET} PRIVATE
    ${CMAKE_SOURCE_DIR}/src/lob
    ${CMAKE_SOURCE_DIR}/src/ingestion
)

target_link_libraries(${TARGET} PRIVATE
    Catch2-static-lib
    back-tester-common
    back-tester-parser
)

catch_discover_tests(${TARGET})