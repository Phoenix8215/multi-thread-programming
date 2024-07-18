#include <chrono>
#include <future>
#include <queue>
#include <string>
#include <vector>
#include "logger.hpp"
#include "model.hpp"
#include "opencv2/opencv.hpp"

using namespace std;

// 20张同样的照片表示一个batch
string imgPaths[20] = {
    "data/source/car.jpg",
    "data/source/airport.jpg",
    "data/source/bedroom.jpg",
    "data/source/crossroad.jpg",
    "data/source/crowd.jpg",
    "data/source/car.jpg",
    "data/source/airport.jpg",
    "data/source/bedroom.jpg",
    "data/source/crossroad.jpg",
    "data/source/crowd.jpg",
    "data/source/car.jpg",
    "data/source/airport.jpg",
    "data/source/bedroom.jpg",
    "data/source/crossroad.jpg",
    "data/source/crowd.jpg",
    "data/source/car.jpg",
    "data/source/airport.jpg",
    "data/source/bedroom.jpg",
    "data/source/crossroad.jpg",
    "data/source/crowd.jpg",
};

int main() {
    logger::set_log_level(logger::LogLevel::Info);

    auto producer = model::create_model(imgPaths, 20);
    auto results = producer->commits();

    vector<future<model::img>> futures;
    for (auto& res : results) {
        futures.push_back(async(launch::async, [&res]() {
            return res.get();
        }));
    }

    // 主线程在等待的过程中可以做其他事情
    bool all_done = false;
    while (!all_done) {
        all_done = true;
        for (auto& future : futures) {
            if (future.wait_for(chrono::milliseconds(0)) == future_status::ready) {
                model::img info = future.get();
                // 处理info
                logger::info("Processed an image.");
            } else {
                all_done = false;
            }
        }
        // 主线程可以继续执行其他任务
        if (!all_done) {
            logger::info("Main thread doing other work...");
            this_thread::sleep_for(chrono::seconds(1));
        }
    }

    producer->stop();
    return 0;
}
