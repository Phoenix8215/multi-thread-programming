#include <chrono>
#include <condition_variable>
#include <future>
#include <iostream>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>
#include "logger.hpp"

using namespace std;

struct Data {
    int id;
    shared_ptr<promise<string>> status;
    string taskType;
};

queue<Data> taskQueue;
mutex mtx_;
condition_variable cv_;
int max_limit = 10;
int global_id = 0;
bool finished = false;

void postprocess_detection(const string& result) {
    LOGV("Postprocessing detection task: %s", result.c_str());
}

void postprocess_segmentation(const string& result) {
    LOGV("Postprocessing segmentation task: %s", result.c_str());
}

void postprocess_depth_estimation(const string& result) {
    LOGV("Postprocessing depth estimation task: %s", result.c_str());
}

void postprocess_pose_estimation(const string& result) {
    LOGV("Postprocessing pose estimation task: %s", result.c_str());
}

void produce(int duration) {
    while (true) {
        LOG(PURPLE "\tProducer produces task %d", global_id);

        // 模拟producer端处理preprocess
        this_thread::sleep_for(chrono::milliseconds(100));
        LOG(DGREEN "\t\tPreprocess finished");

        Data data1, data2, data3, data4;
        {
            unique_lock<mutex> lock(mtx_);
            cv_.wait(lock, [] { return taskQueue.size() + 4 <= max_limit; });

            if (finished) break;  // 检查生产者是否需要退出

            data1.status.reset(new std::promise<string>());
            data2.status.reset(new std::promise<string>());
            data3.status.reset(new std::promise<string>());
            data4.status.reset(new std::promise<string>());

            data1.id = global_id;
            data2.id = global_id;
            data3.id = global_id;
            data4.id = global_id;

            data1.taskType = "detection";
            data2.taskType = "segmentation";
            data3.taskType = "depth estimation";
            data4.taskType = "pose estimation";

            taskQueue.push(data1);
            taskQueue.push(data2);
            taskQueue.push(data3);
            taskQueue.push(data4);

            cv_.notify_all();  // 通知消费者有新的任务
        }

        auto ft1 = data1.status->get_future();
        auto ft2 = data2.status->get_future();
        auto ft3 = data3.status->get_future();
        auto ft4 = data4.status->get_future();

        while (ft1.wait_for(chrono::seconds(0)) != future_status::ready ||
               ft2.wait_for(chrono::seconds(0)) != future_status::ready ||
               ft3.wait_for(chrono::seconds(0)) != future_status::ready ||
               ft4.wait_for(chrono::seconds(0)) != future_status::ready) {
            LOG(DGREEN "\tIn asynchronous waiting, the main thread is doing something else");
            this_thread::sleep_for(chrono::milliseconds(100));  // 模拟主线程做其他事情
        }

        auto st1 = ft1.get();
        auto st2 = ft2.get();
        auto st3 = ft3.get();
        auto st4 = ft4.get();

        // 根据任务类型进行不同的后处理
        postprocess_detection(st1);
        postprocess_segmentation(st2);
        postprocess_depth_estimation(st3);
        postprocess_pose_estimation(st4);

        // 模拟producer端处理postprocess
        this_thread::sleep_for(chrono::milliseconds(duration));
        global_id++;
        LOG(DGREEN "\t\tPostprocess finished");

        LOG(PURPLE "\tFinished task %d, %s, %s, %s, %s\n",
            global_id, st1.c_str(), st2.c_str(), st3.c_str(), st4.c_str());
    }

    {
        unique_lock<mutex> lock(mtx_);
        finished = true;
        cv_.notify_all();  // 通知所有消费者生产者已经完成任务
    }
}

void consume(string label, int duration) {
    while (true) {
        Data cData;
        {
            unique_lock<mutex> lock(mtx_);
            cv_.wait(lock, [] { return !taskQueue.empty() || finished; });

            if (finished && taskQueue.empty()) {
                break;
            }

            cData = taskQueue.front();
            taskQueue.pop();
            cv_.notify_all();  // 通知生产者有空闲空间
        }

        // 模拟消费者处理任务
        this_thread::sleep_for(chrono::milliseconds(duration));
        auto msg = label + "[" + to_string(cData.id) + "]";
        cData.status->set_value(msg);
        LOG(DGREEN "\t\t%s finished task[%d]", label.c_str(), cData.id);
    }
}

int main() {
    thread producer(produce, 500);

    thread consumer1(consume, "detection", 200);
    thread consumer2(consume, "segmentation", 200);
    thread consumer3(consume, "depth estimation", 100);
    thread consumer4(consume, "pose estimation", 100);

    producer.join();
    consumer1.join();
    consumer2.join();
    consumer3.join();
    consumer4.join();

    return 0;
}
