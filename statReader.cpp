#include <bits/stdc++.h>
using namespace std;

int main()
{
    string filename = "/proc/stat";
    string data;
    double prevIdle = 0;
    double prevTotal = 0;
    cout << "CPU Utilization:" << "\n";
    ifstream FileReader(filename);
    cout << std::setprecision(2) << std::fixed;
    while (1)
    {
        FileReader.seekg(0);
        double currUser = 0;
        double currNice = 0;
        double currSystem = 0;
        double currIdle = 0;
        double currIoWait = 0;
        double currIrq = 0;
        double currSoftirq = 0;
        double currSteal = 0;
        string first;
        while (getline(FileReader, data))
        {
            stringstream ss(data);
            ss >> first >> currUser >> currNice >> currSystem >> currIdle >> currIoWait >> currIrq >> currSoftirq >> currSteal;
            break;
        }
        double currTotalIdeal = currIdle + currIoWait;
        double currTotal = currUser + currNice + currSystem + currIdle + currIoWait + currIrq + currSoftirq + currSteal;
        double deltaIdle = currTotalIdeal - prevIdle;
        double deltaTotal = currTotal - prevTotal;
        cout << ((deltaTotal - deltaIdle) / deltaTotal) * 100 << "\n";
        prevIdle = currTotalIdeal;
        prevTotal = currTotal;
        sleep(1);
    }
}
