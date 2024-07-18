#include "model.hpp"
#include "logger.hpp"
#include "utils.hpp"
#include <vector>
#include <future>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <queue>
#include <chrono>
#include "opencv2/highgui.hpp"
#include "opencv2/opencv.hpp"

using namespace std;

namespace model{

struct Job{
    cv::Mat frame;
    shared_ptr<promise<img>> tar;
};

class ModelImpl : public Model{

public:
    ModelImpl(int batchSize):
        m_batchSize(batchSize)
    {};

    ~ModelImpl() {
        stop();
    };

    void stop() {
        if (m_running){
            m_running = false;
            m_cv.notify_all();
        }

        for (int i = 0; i < m_batchSize; i ++){
            if (m_workers[i].joinable()){
                LOGV(DGREEN"[consumer] consumer%d release" CLEAR, i);
                m_workers[i].join();
            }
        }
    }

    bool initialization(){
        m_running = true;

        m_workers.reserve(m_batchSize);
        m_batchedFrames.reserve(m_batchSize);

        for (int i = 0; i < m_batchSize; i ++){
            m_workers.push_back(thread(&ModelImpl::inference, this));
            LOGV(GREEN"[producer]created consumer%d" CLEAR, i);
        }
        return true;
    }

    void forward() override {
        cv::VideoCapture cap("/home/phoenix/workstation/multi-thread-programming/11_cpm_batched_infer/mot_people_medium.mp4");
        if (!cap.isOpened()) {
            LOG("Error opening video stream");
            return;
        }

        while (m_running){
            if (!getBatch(cap)){
                break;
            }

            auto results = commits();
            for (auto& res: results) {
                img info = res.get();
            }
            m_batchedFrames.clear();
        }
        stop();
    }

    bool getBatch(cv::VideoCapture& cap){
        for (int i = 0; i < m_batchSize; i ++) {
            cv::Mat frame;
            cap >> frame;
            if (frame.empty()) {
                return false;
            }
            m_batchedFrames.emplace_back(frame);
        }
        return true;
    }

    vector<shared_future<img>> commits() {
        vector<Job> jobs(m_batchSize);
        vector<shared_future<img>> futures(m_batchSize);

        for (int i = 0; i < m_batchSize; i ++){
            jobs[i].frame = m_batchedFrames[i];
            jobs[i].tar.reset(new promise<img>());
            futures[i] = jobs[i].tar->get_future();
        }

        {
            lock_guard<mutex> lock(m_mtx);
            for (int i = 0; i < m_batchSize; i ++){
                m_jobQueue.emplace(move(jobs[i]));
            }
        }

        m_cv.notify_all();

        LOGV(BLUE"[producer]finished commits" CLEAR);
        return futures;
    }

    void inference() {
        while(m_running){
            Job job;
            img result;

            {
                unique_lock<mutex> lock(m_mtx);

                m_cv.wait(lock, [&](){
                    return !m_running || !m_jobQueue.empty();
                });

                if (!m_running) break;

                job = m_jobQueue.front();
                m_jobQueue.pop();
            }
            LOGV(DGREEN"[consumer] Consumer processing a frame" CLEAR);

            auto  image    = job.frame;
            int   input_w  = image.cols;
            int   input_h  = image.rows;
            int   target_w = 800;
            int   target_h = 800;
            float scale    = min(float(target_w)/input_w, float(target_h)/input_h);
            int   new_w    = int(input_w * scale);
            int   new_h    = int(input_h * scale);
            
            cv::Mat tar(target_w, target_h, CV_8UC3, cv::Scalar(0, 0, 0));
            cv::Mat tmp;
            cv::resize(image, tmp, cv::Size(new_w, new_h));

            int x, y;
            x = (new_w < target_w) ? (target_w - new_w) / 2 : 0;
            y = (new_h < target_h) ? (target_h - new_h) / 2 : 0;

            cv::Rect roi(x, y, new_w, new_h);

            cv::Mat roiOfTar = tar(roi);
            tmp.copyTo(roiOfTar);

            cv::cvtColor(tar, tar, cv::COLOR_BGR2RGB);
            cv::cvtColor(tar, tar, cv::COLOR_RGB2BGR);

            result.path = generateUniquePath();  // Set a generic path for now
            result.data = tar;

            job.tar->set_value(result);
            // cv::imwrite(result.path, result.data);
            LOGV(DGREEN"[consumer] Finished processing, save to %s" CLEAR, result.path.c_str());
        }
    }

private:
    vector<cv::Mat>    m_batchedFrames;
    int                m_batchSize;
    int                m_frameIndex{0};   // 当前帧编号
    queue<Job>         m_jobQueue;
    mutex              m_mtx;
    condition_variable m_cv;
    vector<thread>     m_workers;
    bool               m_running{false};

    string generateUniquePath() {
        ostringstream ss;
        // 如果数字的宽度不足 6 位，空白位置将被填充为零。
        ss << "/home/phoenix/workstation/multi-thread-programming/11_cpm_batched_infer/data/results/frame_" << setw(6) << setfill('0') << m_frameIndex++ << ".png";
        return ss.str();
    }
};

std::shared_ptr<Model> create_model (int batchSize){
    shared_ptr<ModelImpl> ins(new ModelImpl(batchSize));
    if (!ins->initialization())
        ins.reset(); //释放shared_ptr所拥有的对象
    return ins;
}

} //namespace model
