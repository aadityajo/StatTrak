#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <iomanip>
#include <unistd.h>
#include <cstdint>
using namespace std;

struct CoreData
{
    string name;
    uint64_t prevIdle;
    uint64_t prevTotal;
    uint64_t currIdle;
    uint64_t currTotal;
    double cpuUsage;
};

bool readProcStatData(stringstream &ss, CoreData &coreData)
{
    uint64_t user = 0, nice = 0, system = 0, idle = 0, ioWait = 0, irq = 0, softirq = 0, steal = 0;
    string ignoreFirst;
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

void outputCpuUsage(CoreData &coreData)
{
    cout << coreData.name << ": " << coreData.cpuUsage << " ";
}

int main()
{
    string filename = "/proc/stat";
    string data;
    ifstream fileReader(filename);

    if (!fileReader.is_open())
    {
        cout << "Failed to open /proc/stat" << "\n";
        return 1;
    }

    cout << "CPU Utilization (%):" << "\n";
    cout << std::setprecision(2) << std::fixed;
    vector<CoreData> coreDataArr;
    string name;
    uint64_t user, nice, system, idle, ioWait, irq, softirq, steal;
    while (getline(fileReader, data))
    {
        stringstream ss(data);
        ss >> name >> user >> nice >> system >> idle >> ioWait >> irq >> softirq >> steal;
        if (name.find("cpu") == string::npos)
            break;
        CoreData coreData;
        coreData.name = name;
        coreData.prevIdle = idle + ioWait;
        coreData.prevTotal = user + nice + system + idle + ioWait + irq + softirq + steal;
        coreDataArr.push_back(coreData);
    }
    sleep(1);

    while (1)
    {
        fileReader.seekg(0);
        for (auto &coreData : coreDataArr)
        {
            getline(fileReader, data);
            stringstream ss(data);
            if (readProcStatData(ss, coreData))
            {
                calculateCpuUsage(coreData);
                outputCpuUsage(coreData);
            }
            else
            {
                cout << "ERR: " << coreData.name << " ";
            }
        }
        cout << "\n";
        sleep(1);
    }
}
