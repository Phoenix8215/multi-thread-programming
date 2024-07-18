#include "logger.hpp"
#include "model.hpp"
#include "timer.hpp"
#include "opencv2/opencv.hpp"
#include <string>

using namespace std;

int main(){
    logger::set_log_level(logger::LogLevel::Info);

    timer::Timer timer;

    int    batchSize = 32;

    auto   producer  = model::create_model(batchSize);

    // main端只需要调用一个forward就好了
    timer.start_cpu();
    producer->forward();
    timer.stop_cpu();

    // timer.duration_cpu<timer::Timer::ms>("In total");
    timer.throughput_cpu<timer::Timer::s>("Batched inference", 1000);

}
