#pragma once

#include "common/BasicTypes.hpp"
#include <unordered_map>

namespace cmf {

/// Сопоставляет OrderId → SecurityId (инструмент).
/// Инструмент становится известен только при добавлении заявки.
class OrderInstrumentMap {
public:
    void recordAdd(OrderId oid, SecurityId iid) { map_[oid] = iid; }
    SecurityId get(OrderId oid) const {
        auto it = map_.find(oid);
        if (it != map_.end()) return it->second;
        return 0;   // или бросить исключение
    }
private:
    std::unordered_map<OrderId, SecurityId> map_;
};

} // namespace cmf