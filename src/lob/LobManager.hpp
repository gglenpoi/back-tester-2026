#pragma once

#include "LimitOrderBook.hpp"
#include "OrderInstrumentMap.hpp"
#include "ingestion/MarketDataEvent.hpp"

#include <unordered_map>
#include <iostream>

namespace cmf {

class LobManager {
public:
    void processEvent(const MarketDataEvent& event);
    void printAllSnapshots(std::ostream& os = std::cout) const;

    // Доступ к книге конкретного инструмента (для отчёта)
    const LimitOrderBook* getLob(SecurityId id) const {
        auto it = lobs_.find(id);
        return (it != lobs_.end()) ? &it->second : nullptr;
    }

    auto begin() const { return lobs_.begin(); }
    auto end()   const { return lobs_.end(); }

private:
    std::unordered_map<SecurityId, LimitOrderBook> lobs_;
    OrderInstrumentMap oim_;
};

} // namespace cmf