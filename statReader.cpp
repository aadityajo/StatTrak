#include <bits/stdc++.h>
using namespace std;

struct CoreData
{
    string name;
    uint64_t prevIdle;
    uint64_t prevTotal;
    uint64_t currIdle;
    uint64_t currTotal;
};

void readProcStatData(ifstream &reader, CoreData &coreData)
{
    uint64_t user, nice, system, idle, ioWait, irq, softirq, steal;
    string data, ignoreFirst;
    getline(reader, data);
    stringstream ss(data);
    ss >> ignoreFirst >> user >> nice >> system >> idle >> ioWait >> irq >> softirq >> steal;
    coreData.currIdle = idle + ioWait;
    coreData.currTotal = user + nice + system + idle + ioWait + irq + softirq + steal;
}

void outputCpuUsage(CoreData &coreData)
{
    double deltaIdle = coreData.currIdle - coreData.prevIdle;
    double deltaTotal = coreData.currTotal - coreData.prevTotal;
    cout << coreData.name << ": ";
    if (deltaTotal > 0)
        cout << ((double)(deltaTotal - deltaIdle) / deltaTotal) * 100 << " ";
    else
        cout << 0.0 << " ";
    coreData.prevTotal = coreData.currTotal;
    coreData.prevIdle = coreData.currIdle;
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
            readProcStatData(fileReader, coreData);
            outputCpuUsage(coreData);
        }
        cout << "\n";
        sleep(1);
    }
}
