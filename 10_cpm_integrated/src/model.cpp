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

// img记录了图片数据和他们相对应的路径
struct Job{
    img src;
    shared_ptr<promise<img>> tar;
};

class ModelImpl : public Model{

public:
    ModelImpl(string* img_list, int batchSize):
        m_imgPaths(img_list), m_batchSize(batchSize){};

    /* 
     * 析构函数:
     *  当ModelImpl可以释放资源的时候，也自动的把线程同步
     */
    ~ModelImpl() {
        stop();
    };

    void stop() override{
        /* 如果正在执行，那么就停止 */
        if (m_running){
            m_running = false;
            // notify_all 被用来通知所有等待在条件变量 m_cv 上的线程，
            // 这些线程可能正在等待某个条件（例如，队列中有新任务可处理）。
            // notify_all 唤醒所有等待的线程，以便它们能够重新评估等待条件，并且在发现 m_running 
            // 被设置为 false 后，可以安全地退出等待循环和终止它们的工作
            m_cv.notify_all();
        }

        /* 对于所有线程进行join处理 */
        for (int i = 0; i < m_batchSize; i ++){
            if (m_workers[i].joinable()){
                LOGV(DGREEN"[consumer] consumer%d release" CLEAR, i);
                m_workers[i].join();
            }
        }
    }

    /*
     * 初始化:
     *  创建消费者所用的线程，个数为batchSize个
     *  为消费者线程绑定消费者函数
    */
    bool initialization(){
        m_running = true;
        m_workers.reserve(m_batchSize);
        for (int i = 0; i < m_batchSize; i ++){
            // 在多线程环境中，成员函数 inference 必须知道它操作的是哪个对象实例。
            // 因此，需要将 this 指针传递给线程，以便线程能够正确地调用成员函数并访问对象的成员变量。
            m_workers.emplace_back(thread(&ModelImpl::inference, this));
            LOGV(GREEN"[producer]created consumer%d" CLEAR, i);
        }
        return true;
    }

    /* 
     * 生产者: producer
     *  负责分配job, 每一个job负责管理一个图片数据，以及对应的future
     *  返回一个future的vector, 分配对应每一个job
    */
    vector<shared_future<img>> commits() override{
        vector<Job> jobs(m_batchSize);
        vector<shared_future<img>> futures(m_batchSize);

        /* 设置一批job, 并对每一个job的promise设置对应的future */
        for (int i = 0; i < m_batchSize; i ++){
            jobs[i].src.data = cv::imread(m_imgPaths[i]);
            jobs[i].src.path = m_imgPaths[i];
            jobs[i].tar.reset(new promise<img>());
            futures[i] = jobs[i].tar->get_future();
        }

        /* 对jobQueue进行原子push */
        {
            lock_guard<mutex> lock(m_mtx);
            for (int i = 0; i < m_batchSize; i ++){
                m_jobQueue.emplace(move(jobs[i]));
            }
        }

        /* 唤醒所有堵塞的consumer */
        m_cv.notify_all();

        LOGV(BLUE"[producer]finished commits" CLEAR);
        return futures;
    }

    /* 
     * 消费者: consumer
     *  消费者是一直在启动着的
     *  只要jobQueue有数据，就处理job, 并更新job内部的promise
     *  可以多个consumer处理同一个jobQueue
    */
    void inference() {
        while(m_running){
            Job job;
            img result;

            /* 对jobQueue进行原子pop */
            {
                unique_lock<mutex> lock(m_mtx);

                /* 
                 * consumer不被阻塞只有两种情况：
                 *  1. jobQueue不是空，这个时候需要consumer去consume
                 *  2. model已经被析构了，所有的线程都需要停止
                 *
                 * 所以，相应的condition_variable的notify也需要有两个
                 *  1. 当jobQueue已经填好了数据的时候
                 *  2. 当model已经被析构的时候
                 */
                m_cv.wait(lock, [&](){
                    return !m_running || !m_jobQueue.empty();
                });

                /* 如果model已经被析构了，就跳出循环, 不再consume了*/
                if (!m_running) break;

                job = m_jobQueue.front();
                m_jobQueue.pop();
            }
            LOGV(DGREEN"[consumer] Consumer processing %s" CLEAR, job.src.path.c_str());

            /*
             * 对取出来的job数据进行处理，并更新promise
             * 这里面对应着batched inference之后的各个task的postprocess
             * 由于相比于GPU上的操作，这里的postprocess一般会在CPU上操作，所以会比较慢
             * 因此可以选择考虑多线程异步执行
             * 这里为了达到耗时效果，选择在CPU端做如下操作:
             *  1. letterbox resize
             *  2. bgr2rgb
             *  3. rgb2bgr
             */
            auto image = job.src.data;

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

            result.path = changePath(job.src.path, "../results", ".png", "letter_box");
            result.data = tar;

            job.tar->set_value(result);
            cv::imwrite(result.path, result.data);
            LOG(DGREEN"[consumer] Finished processing, save to %s" CLEAR, job.src.path.c_str());
        }
    }


private:
    string*            m_imgPaths;
    int                m_batchSize;
    queue<Job>         m_jobQueue;
    mutex              m_mtx;
    condition_variable m_cv;
    vector<thread>     m_workers;
    bool               m_running{false};
};

// RAII模式对实现类进行资源获取即初始化
std::shared_ptr<Model> create_model (std::string* img_list, int batchSize){
    shared_ptr<ModelImpl> ins(new ModelImpl(img_list, batchSize));
    if (!ins->initialization())
        ins.reset(); //释放shared_ptr所拥有的对象
    return ins;
}

} //namespace model
