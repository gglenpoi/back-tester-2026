#include "LobManager.hpp"

namespace cmf {

void LobManager::processEvent(const MarketDataEvent& event) {
    SecurityId instr = 0;
    if (event.action == Action::Add) {
        instr = event.instrumentId;
        oim_.recordAdd(event.orderId, instr);
    } else {
        instr = oim_.get(event.orderId);
        if (instr == 0) {
            std::cerr << "Unknown orderId " << event.orderId 
                      << " in non-Add event, skipping\n";
            return;
        }
    }

    auto& lob = lobs_[instr];
    switch (event.action) {
        case Action::Add:    lob.applyAdd(event.orderId, event.side, 
                                          event.price, event.size); break;
        case Action::Delete: lob.applyCancel(event.orderId); break;
        case Action::Modify: lob.applyModify(event.orderId, event.size); break;
        case Action::Trade:  lob.applyTrade(event.orderId, event.size); break;
        default: break;
    }
}

void LobManager::printAllSnapshots(std::ostream& os) const {
    for (const auto& [id, lob] : lobs_) {
        os << "=== Instrument " << id << " ===\n";
        lob.printSnapshot(os);
        os << "BestBid: " << lob.bestBid().value_or(-1) 
           << ", BestAsk: " << lob.bestAsk().value_or(-1) << "\n\n";
    }
}

} // namespace cmf