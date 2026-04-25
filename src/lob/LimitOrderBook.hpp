#pragma once

#include "common/BasicTypes.hpp"
#include "ingestion/MarketDataEvent.hpp"

#include <map>
#include <unordered_map>
#include <functional>
#include <optional>
#include <iostream>

namespace cmf {

class LimitOrderBook {
public:
    void applyAdd(OrderId oid, Side side, Price price, Quantity size);
    void applyCancel(OrderId oid);
    void applyModify(OrderId oid, Quantity newSize);
    void applyTrade(OrderId oid, Quantity tradedQty);
    void applyFill(OrderId oid); 

    std::optional<Price> bestBid() const;
    std::optional<Price> bestAsk() const;
    Quantity volumeAtPrice(Side side, Price price) const;

    void printSnapshot(std::ostream& os = std::cout, size_t levels = 5) const;

private:
    struct OrderInfo {
        Side side;
        Price price;
        Quantity size;
    };

    std::map<Price, Quantity, std::greater<Price>> bids_;
    std::map<Price, Quantity, std::less<Price>>    asks_;

    std::unordered_map<OrderId, OrderInfo> orders_;

    void removeOrderFromBook(const OrderInfo& info);
};

} // namespace cmf