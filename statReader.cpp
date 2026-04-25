#include <iostream>
#include <queue>
#include <condition_variable>
#include <fstream>
#include <pthread.h>
#include <thread>
#include <sstream>
#include <string>
#include <vector>
#include <iomanip>
#include <unistd.h>
#include <cstdint>
#include <csignal>
#include <cstdlib>
#include <atomic>
#include <mutex>

std::mutex shutdownMutex, metricDataQueueMutex;
std::condition_variable shutdownCV;
std::condition_variable metricQueueCV;
std::atomic<bool> shutdownRequested = false;

struct CoreData
{
    std::string name;
    uint64_t prevIdle;
    uint64_t prevTotal;
    uint64_t currIdle;
    uint64_t currTotal;
    double cpuUsage;
};

struct MetricData
{
    std::string name;
    double val;

    MetricData(std::string name, double val)
    {
        this->name = name;
        this->val = val;
    };
};

std::queue<MetricData> metricDataQ;

void shutdownHelper()
{
    shutdownRequested.store(true);
    shutdownCV.notify_all();
    metricQueueCV.notify_all();
}

bool readProcStatData(std::stringstream &ss, CoreData &coreData)
{
    uint64_t user = 0, nice = 0, system = 0, idle = 0, ioWait = 0, irq = 0, softirq = 0, steal = 0;
    std::string ignoreFirst;
    if (!(ss >> ignoreFirst >> user >> nice >> system >> idle))
    {
        return 0;
    }
    ss >> ioWait >> irq >> softirq >> steal;
    coreData.currIdle = idle + ioWait;
    coreData.currTotal = user + nice + system + idle + ioWait + irq + softirq + steal;
    return 1;
}

void calculateCpuUsage(CoreData &coreData)
{
    double deltaIdle = coreData.currIdle - coreData.prevIdle;
    double deltaTotal = coreData.currTotal - coreData.prevTotal;
    if (deltaTotal > 0)
        coreData.cpuUsage = ((double)(deltaTotal - deltaIdle) / deltaTotal) * 100;
    else
        coreData.cpuUsage = 0.0;
    coreData.prevTotal = coreData.currTotal;
    coreData.prevIdle = coreData.currIdle;
}

void pushCpuUsage(CoreData &coreData)
{
    std::lock_guard<std::mutex> lock(metricDataQueueMutex);
    metricDataQ.push(MetricData(coreData.name, coreData.cpuUsage));
}

void runStatTrackCPU()
{
    std::string filename = "/proc/stat";
    std::string data;
    std::ifstream fileReader(filename);

    if (!fileReader.is_open())
    {
        shutdownHelper();
        return;
    }

    std::vector<CoreData> coreDataArr;
    std::string name;
    uint64_t user, nice, system, idle, ioWait, irq, softirq, steal;
    while (getline(fileReader, data))
    {
        std::stringstream ss(data);
        ss >> name >> user >> nice >> system >> idle >> ioWait >> irq >> softirq >> steal;
        if (name.find("cpu") == std::string::npos)
            break;
        CoreData coreData;
        coreData.name = name;
        coreData.prevIdle = idle + ioWait;
        coreData.prevTotal = user + nice + system + idle + ioWait + irq + softirq + steal;
        coreDataArr.push_back(coreData);
    }
    fileReader.close();
    {
        std::unique_lock<std::mutex> lock{shutdownMutex};
        shutdownCV.wait_for(lock, std::chrono::seconds(1), []
                            { return shutdownRequested.load(); });
    }

    while (!shutdownRequested.load())
    {
        fileReader.open(filename);
        for (auto &coreData : coreDataArr)
        {
            getline(fileReader, data);
            std::stringstream ss(data);
            if (readProcStatData(ss, coreData))
            {
                calculateCpuUsage(coreData);
                pushCpuUsage(coreData);
            }
        }
        metricQueueCV.notify_one();
        {
            std::unique_lock<std::mutex> lock{shutdownMutex};
            shutdownCV.wait_for(lock, std::chrono::seconds(1), []
                                { return shutdownRequested.load(); });
        }

        fileReader.close();
    }
}

void runStatTrackExporter()
{
    std::unique_lock<std::mutex> metricDataQueueLock{metricDataQueueMutex, std::defer_lock};
    while (!shutdownRequested.load())
    {
        metricDataQueueLock.lock();
        if (!metricDataQ.empty())
        {
            while (!metricDataQ.empty())
            {
                MetricData &curr = metricDataQ.front();
                std::cout << curr.name << " " << curr.val << " ";
                metricDataQ.pop();
            }
            std::cout << "\n";
        }
        metricDataQueueLock.unlock();
        {
            std::unique_lock<std::mutex> lock{shutdownMutex};
            metricQueueCV.wait_for(lock, std::chrono::seconds(1), []
                                   { return shutdownRequested.load() || !metricDataQ.empty(); });
        }
    }
}

int main()
{
    // Ref : https://thomastrapp.com/blog/signal-handlers-for-multithreaded-cpp/
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGINT);
    sigaddset(&sigset, SIGTERM);

    pthread_sigmask(SIG_BLOCK, &sigset, nullptr);
    std::cout << std::setprecision(2) << std::fixed;
    std::thread signal_thread([&]
                              {
        int signum;
        sigwait(&sigset, &signum); 
        shutdownHelper(); });

    std::thread cpuReaderThread(runStatTrackCPU);
    std::thread exporterThread(runStatTrackExporter);

    cpuReaderThread.join();
    exporterThread.join();
    signal_thread.join();
    std::cout << "\nExited" << "\n";
}
