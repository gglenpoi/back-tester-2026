#include "common/MarketDataEvent.hpp"
#include "common/LimitOrderBook.hpp"
#include "common/OrderInstrumentMap.hpp"

#include <fstream>
#include <iostream>
#include <queue>
#include <vector>
#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <cmath>
#include <stdexcept>

#include <nlohmann/json.hpp>

using json = nlohmann::json;
using namespace cmf;

// ---------- timestamp parsing ----------
NanoTime parseDatabentoTimestamp(const std::string& ts) {
    auto dotPos = ts.find('.');
    if (dotPos == std::string::npos) {
        std::tm tm = {};
        std::string s = ts.substr(0, ts.size()-1); // remove 'Z'
        std::istringstream ss(s);
        ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
        auto tp = std::chrono::system_clock::from_time_t(timegm(&tm));
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
                   tp.time_since_epoch()).count();
    }
    std::string secPart = ts.substr(0, dotPos) + "Z";
    std::string nanoStr = ts.substr(dotPos+1, ts.size()-dotPos-2);
    std::tm tm = {};
    std::istringstream ss(secPart);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    auto tp = std::chrono::system_clock::from_time_t(timegm(&tm));
    auto secNanos = std::chrono::duration_cast<std::chrono::nanoseconds>(
                        tp.time_since_epoch()).count();
    int64_t frac = std::stoll(nanoStr);
    return secNanos + frac;
}

struct FileEvent {
    NanoTime ts;
    MarketDataEvent event;
    size_t fileIndex;
    bool operator>(const FileEvent& other) const { return ts > other.ts; }
};

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <file1.json> [file2.json ...]\n";
        return 1;
    }

    std::vector<std::string> fileNames;
    for (int i = 1; i < argc; ++i) fileNames.push_back(argv[i]);

    std::vector<std::ifstream> files(fileNames.size());
    std::priority_queue<FileEvent, std::vector<FileEvent>, std::greater<>> minHeap;

    // Open files and push first valid event from each
    for (size_t i = 0; i < fileNames.size(); ++i) {
        files[i].open(fileNames[i]);
        if (!files[i].is_open()) {
            std::cerr << "Failed to open " << fileNames[i] << "\n";
            return 1;
        }
        std::string line;
        bool gotEvent = false;
        while (std::getline(files[i], line)) {
            if (line.empty()) continue;
            try {
                json j = json::parse(line);
                std::string actionStr = j.value("action", "");
                if (actionStr == "R") continue;

                MarketDataEvent ev;
                ev.timestamp = parseDatabentoTimestamp(j["hd"]["ts_event"]);
                ev.order_id = std::stoull(j.value("order_id", "0"));
                ev.instr_id = j["hd"].value("instrument_id", 0U);
                std::string sideStr = j.value("side", "");
                if (sideStr == "B") ev.side = Side::Buy;
                else if (sideStr == "S") ev.side = Side::Sell;
                else ev.side = Side::None;
                ev.price = j.value("price", 0.0);
                ev.size  = j.value("size", 0.0);

                if (actionStr == "A") ev.action = OrderAction::Add;
                else if (actionStr == "C") ev.action = OrderAction::Cancel;
                else if (actionStr == "M") ev.action = OrderAction::Modify;
                else if (actionStr == "T") ev.action = OrderAction::Trade;
                else if (actionStr == "F") ev.action = OrderAction::Fill;
                else continue;

                minHeap.push({ev.timestamp, std::move(ev), i});
                gotEvent = true;
                break;
            } catch (...) {
                continue;
            }
        }
        if (!gotEvent) {
            files[i].close(); // no valid events in file
        }
    }

    OrderInstrumentMap oim;
    std::unordered_map<InstrumentId, LimitOrderBook> lobs;
    std::vector<MarketDataEvent> firstEvents;
    std::vector<MarketDataEvent> lastEvents;
    const size_t maxKeep = 10;
    size_t totalEvents = 0;
    NanoTime firstTimestamp = 0;
    NanoTime lastTimestamp = 0;

    auto start = std::chrono::high_resolution_clock::now();
    size_t snapshotCount = 0;

    while (!minHeap.empty()) {
        FileEvent fe = minHeap.top();
        minHeap.pop();
        MarketDataEvent ev = std::move(fe.event);
        size_t fileIdx = fe.fileIndex;

        // track first/last
        if (totalEvents == 0) firstTimestamp = ev.timestamp;
        lastTimestamp = ev.timestamp;
        if (firstEvents.size() < maxKeep) firstEvents.push_back(ev);
        // keep last 10 in a circular buffer
        if (lastEvents.size() >= maxKeep) lastEvents.erase(lastEvents.begin());
        lastEvents.push_back(ev);
        ++totalEvents;

        // Determine instrument: for Add, record; else lookup.
        InstrumentId instr = ev.instr_id;
        if (ev.action == OrderAction::Add) {
            oim.record(ev.order_id, instr);
        } else {
            if (instr == 0) { // not directly provided in event
                if (oim.contains(ev.order_id)) {
                    instr = oim.get(ev.order_id);
                } else {
                    std::cerr << "Warning: unknown order_id " << ev.order_id
                              << " for non-Add event, skipping.\n";
                    goto nextEvent;
                }
            }
        }

        {
            auto& lob = lobs[instr];
            switch (ev.action) {
                case OrderAction::Add:
                    lob.applyAdd(ev.order_id, ev.side, ev.price, ev.size);
                    break;
                case OrderAction::Cancel:
                    lob.applyCancel(ev.order_id);
                    break;
                case OrderAction::Modify:
                    lob.applyModify(ev.order_id, ev.size);
                    break;
                case OrderAction::Trade:
                    lob.applyTrade(ev.order_id, ev.size); // size is traded quantity
                    break;
                case OrderAction::Fill:
                    lob.applyFill(ev.order_id);
                    break;
                default: break;
            }
        }

        // periodic snapshot every 1,000,000 events
        if (totalEvents % 1'000'000 == 0) {
            std::cout << "\nSnapshot after " << totalEvents << " events:\n";
            for (const auto& [id, lob] : lobs) {
                std::cout << "Instrument " << id << ":\n";
                lob.printSnapshot(std::cout);
            }
            ++snapshotCount;
        }

nextEvent:
        // read next event from same file
        std::string line;
        while (std::getline(files[fileIdx], line)) {
            if (line.empty()) continue;
            try {
                json j = json::parse(line);
                std::string actionStr = j.value("action", "");
                if (actionStr == "R") continue;

                MarketDataEvent nextEv;
                nextEv.timestamp = parseDatabentoTimestamp(j["hd"]["ts_event"]);
                nextEv.order_id = std::stoull(j.value("order_id", "0"));
                nextEv.instr_id = j["hd"].value("instrument_id", 0U);
                std::string sideStr = j.value("side", "");
                if (sideStr == "B") nextEv.side = Side::Buy;
                else if (sideStr == "S") nextEv.side = Side::Sell;
                else nextEv.side = Side::None;
                nextEv.price = j.value("price", 0.0);
                nextEv.size  = j.value("size", 0.0);

                if (actionStr == "A") nextEv.action = OrderAction::Add;
                else if (actionStr == "C") nextEv.action = OrderAction::Cancel;
                else if (actionStr == "M") nextEv.action = OrderAction::Modify;
                else if (actionStr == "T") nextEv.action = OrderAction::Trade;
                else if (actionStr == "F") nextEv.action = OrderAction::Fill;
                else continue;

                minHeap.push({nextEv.timestamp, std::move(nextEv), fileIdx});
                break;
            } catch (...) {
                continue;
            }
        }
        // if file exhausted, minHeap will gradually empty
    }

    auto end = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double>(end - start).count();

    // Output first and last 10 events
    std::cout << "\n=== First " << firstEvents.size() << " events ===\n";
    for (const auto& ev : firstEvents) std::cout << ev.toString() << "\n";
    std::cout << "\n=== Last " << lastEvents.size() << " events ===\n";
    for (const auto& ev : lastEvents) std::cout << ev.toString() << "\n";

    // Final LOB state for each instrument
    std::cout << "\n=== Final LOB state ===\n";
    for (const auto& [id, lob] : lobs) {
        std::cout << "Instrument " << id << ":\n";
        auto bb = lob.bestBid();
        auto ba = lob.bestAsk();
        if (bb) std::cout << "  Best bid: " << *bb << "\n";
        if (ba) std::cout << "  Best ask: " << *ba << "\n";
        lob.printSnapshot(std::cout);
    }

    std::cout << "\n=== Statistics ===\n";
    std::cout << "Total events processed: " << totalEvents << "\n";
    if (totalEvents > 0) {
        std::cout << "First timestamp: " << firstTimestamp << "\n";
        std::cout << "Last timestamp:  " << lastTimestamp << "\n";
    }
    std::cout << "Processing time: " << elapsed << " seconds\n";
    std::cout << "Events/second: " << (totalEvents / elapsed) << "\n";

    return 0;
}