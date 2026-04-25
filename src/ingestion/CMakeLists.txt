set(TARGET back-tester-parser)

file(GLOB SRC CONFIGURE_DEPENDS 
    JsonParser.cpp 
    JsonParser.hpp 
    MarketDataEvent.hpp
)

add_library(${TARGET} STATIC ${SRC})
target_include_directories(${TARGET} PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(${TARGET} PUBLIC back-tester-common)

add_executable(back-tester-ingestion IngestionApp.cpp)
target_link_libraries(back-tester-ingestion PRIVATE back-tester-parser)