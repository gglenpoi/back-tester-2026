#include "LobManager.hpp"
#include "ingestion/JsonParser.hpp"

#include <fstream>
#include <iostream>
#include <chrono>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>

using namespace cmf;
using namespace std::chrono;

/// Простая потокобезопасная очередь
template<typename T>
class SyncQueue {
public:
    void push(T val) {
        std::lock_guard<std::mutex> lock(m_);
        q_.push(std::move(val));
        cv_.notify_one();
    }
    bool tryPop(T& val) {
        std::unique_lock<std::mutex> lock(m_);
        if (q_.empty()) return false;
        val = std::move(q_.front());
        q_.pop();
        return true;
    }
    void waitAndPop(T& val) {
        std::unique_lock<std::mutex> lock(m_);
        cv_.wait(lock, [this]{ return !q_.empty(); });
        val = std::move(q_.front());
        q_.pop();
    }
    bool empty() const {
        std::lock_guard<std::mutex> lock(m_);
        return q_.empty();
    }
private:
    mutable std::mutex m_;
    std::queue<T> q_;
    std::condition_variable cv_;
};

/// Поток-продюсер: читает файл, парсит и кладёт в очередь
void producer(const std::string& filename, SyncQueue<MarketDataEvent>& queue) {
    std::ifstream file(filename);
    if (!file) {
        std::cerr << "Cannot open file: " << filename << "\n";
        queue.push(MarketDataEvent{});   // сигнал конца
        return;
    }
    std::string line;
    while (std::getline(file, line)) {
        auto evt = parseLine(line);
        if (evt) {
            queue.push(std::move(*evt));
        }
    }
    // Сигнал конца – нулевой timestamp
    MarketDataEvent sentinel{};   // все поля становятся нулевыми
    // Явно устанавливаем timestamp = 0 (уже и так 0 после {}, но для ясности можно оставить)
    sentinel.timestamp = 0;
    queue.push(sentinel);
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <json_file>\n";
        return 1;
    }

    SyncQueue<MarketDataEvent> queue;

    // Запускаем продюсер
    std::thread prod(producer, argv[1], std::ref(queue));

    LobManager mgr;
    size_t totalEvents = 0;
    NanoTime lastSnapshotTs = 0;
    const NanoTime snapshotInterval = 100'000'000; // 100 ms в наносекундах

    auto startTime = high_resolution_clock::now();

    while (true) {
        MarketDataEvent ev{};
        queue.waitAndPop(ev);
        if (ev.timestamp == 0) break;   // сантинел

        // Периодический снимок
        if (totalEvents == 0 || ev.timestamp - lastSnapshotTs >= snapshotInterval) {
            std::cout << "\n--- Snapshot @ " << ev.timestamp << " ns ---\n";
            mgr.printAllSnapshots();
            lastSnapshotTs = ev.timestamp;
        }

        mgr.processEvent(ev);
        ++totalEvents;
    }

    auto endTime = high_resolution_clock::now();
    prod.join();

    // Финальное состояние
    std::cout << "\n===== FINAL STATE =====\n";
    mgr.printAllSnapshots();

    // Статистика
    std::cout << "\n===== STATISTICS =====\n";
    std::cout << "Total events processed: " << totalEvents << "\n";
    double elapsedSec = duration<double>(endTime - startTime).count();
    std::cout << "Processing time: " << elapsedSec << " s\n";
    if (elapsedSec > 0)
        std::cout << "Events per second: " << totalEvents / elapsedSec << "\n";

    return 0;
}