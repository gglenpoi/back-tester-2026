#pragma once

#include "common/BasicTypes.hpp"
#include <string>
#include <cstdint>

namespace cmf {

enum class OrderAction : int8_t {
    None = 0,
    Add,
    Cancel,
    Modify,
    Trade,
    Fill
};

inline std::string to_string(OrderAction action) {
    switch (action) {
        case OrderAction::Add:    return "ADD";
        case OrderAction::Cancel: return "CANCEL";
        case OrderAction::Modify: return "MODIFY";
        case OrderAction::Trade:  return "TRADE";
        case OrderAction::Fill:   return "FILL";
        default:                  return "NONE";
    }
}

using InstrumentId = uint32_t;

struct MarketDataEvent {
    NanoTime       timestamp = 0;
    OrderId        order_id = 0;
    InstrumentId   instr_id = 0;   // instrument_id from Databento, 0 if unknown
    Side           side = Side::None;
    Price          price = 0.0;
    Quantity       size = 0.0;
    OrderAction    action = OrderAction::None;

    std::string toString() const {
        return std::to_string(timestamp) + " " +
               std::to_string(order_id) + " " +
               std::to_string(instr_id) + " " +
               to_string(side) + " " +
               std::to_string(price) + " " +
               std::to_string(size) + " " +
               to_string(action);
    }
};

} // namespace cmf