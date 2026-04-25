#include "LimitOrderBook.hpp"

namespace cmf {

void LimitOrderBook::removeOrderFromBook(const OrderInfo& info) {
    if (info.side == Side::Buy) {
        auto it = bids_.find(info.price);
        if (it != bids_.end()) {
            it->second -= info.size;
            if (it->second <= 0) bids_.erase(it);
        }
    } else if (info.side == Side::Sell) {
        auto it = asks_.find(info.price);
        if (it != asks_.end()) {
            it->second -= info.size;
            if (it->second <= 0) asks_.erase(it);
        }
    }
}

void LimitOrderBook::applyAdd(OrderId oid, Side side, Price price, Quantity size) {
    // Удаляем, если заявка уже существует (защита)
    if (orders_.count(oid)) {
        applyCancel(oid);
    }
    orders_[oid] = {side, price, size};
    if (side == Side::Buy)
        bids_[price] += size;
    else if (side == Side::Sell)
        asks_[price] += size;
}

void LimitOrderBook::applyCancel(OrderId oid) {
    auto it = orders_.find(oid);
    if (it == orders_.end()) return;
    removeOrderFromBook(it->second);
    orders_.erase(it);
}

void LimitOrderBook::applyModify(OrderId oid, Quantity newSize) {
    auto it = orders_.find(oid);
    if (it == orders_.end()) return;
    Quantity delta = newSize - it->second.size;
    if (delta == 0) return;

    it->second.size = newSize;
    if (it->second.side == Side::Buy) {
        bids_[it->second.price] += delta;
        if (bids_[it->second.price] <= 0) bids_.erase(it->second.price);
    } else {
        asks_[it->second.price] += delta;
        if (asks_[it->second.price] <= 0) asks_.erase(it->second.price);
    }
    if (newSize <= 0) orders_.erase(it);
}

void LimitOrderBook::applyTrade(OrderId oid, Quantity tradedQty) {
    auto it = orders_.find(oid);
    if (it == orders_.end()) return;
    Quantity remaining = it->second.size - tradedQty;
    if (remaining <= 0) {
        applyCancel(oid);
    } else {
        applyModify(oid, remaining);
    }
}

void LimitOrderBook::applyFill(OrderId oid) {
    applyCancel(oid);
}

std::optional<Price> LimitOrderBook::bestBid() const {
    if (bids_.empty()) return std::nullopt;
    return bids_.begin()->first;
}

std::optional<Price> LimitOrderBook::bestAsk() const {
    if (asks_.empty()) return std::nullopt;
    return asks_.begin()->first;
}

Quantity LimitOrderBook::volumeAtPrice(Side side, Price price) const {
    if (side == Side::Buy) {
        auto it = bids_.find(price);
        return (it != bids_.end()) ? it->second : 0;
    } else {
        auto it = asks_.find(price);
        return (it != asks_.end()) ? it->second : 0;
    }
}

void LimitOrderBook::printSnapshot(std::ostream& os, size_t levels) const {
    os << "----- LOB Snapshot -----\n";
    os << "Bids (best first):\n";
    size_t cnt = 0;
    for (const auto& [price, qty] : bids_) {
        os << "  " << price << " -> " << qty << "\n";
        if (++cnt >= levels) break;
    }
    os << "Asks (best first):\n";
    cnt = 0;
    for (const auto& [price, qty] : asks_) {
        os << "  " << price << " -> " << qty << "\n";
        if (++cnt >= levels) break;
    }
    os << "------------------------\n";
}

} // namespace cmf