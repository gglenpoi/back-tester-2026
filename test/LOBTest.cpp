#include "lob/LimitOrderBook.hpp"
#include "catch2/catch_all.hpp"

using namespace cmf;

TEST_CASE("LimitOrderBook basic add/cancel") {
    LimitOrderBook lob;
    lob.applyAdd(1, Side::Buy, 100.0, 10);
    lob.applyAdd(2, Side::Buy, 101.0, 5);
    lob.applyAdd(3, Side::Sell, 102.0, 20);

    REQUIRE(lob.bestBid() == 101.0);
    REQUIRE(lob.bestAsk() == 102.0);
    REQUIRE(lob.volumeAtPrice(Side::Buy, 100.0) == 10);

    lob.applyCancel(1);
    REQUIRE(lob.bestBid() == 101.0);
    REQUIRE(lob.volumeAtPrice(Side::Buy, 100.0) == 0);

    lob.applyModify(3, 15);
    REQUIRE(lob.volumeAtPrice(Side::Sell, 102.0) == 15);
}

TEST_CASE("LimitOrderBook modify and trade") {
    LimitOrderBook lob;
    lob.applyAdd(10, Side::Buy, 200.0, 100);
    lob.applyModify(10, 60);   // уменьшили
    REQUIRE(lob.volumeAtPrice(Side::Buy, 200.0) == 60);

    lob.applyTrade(10, 40);    // исполнили 40, осталось 20
    REQUIRE(lob.volumeAtPrice(Side::Buy, 200.0) == 20);
}