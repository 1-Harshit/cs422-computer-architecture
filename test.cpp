#include <iostream>
#include <unordered_map>
typedef unsigned long long UINT64;
typedef unsigned int UINT32;

using namespace std;
void AnalysisMetrics(void *insAddr, UINT32 insSize)
{
    // assume data access to be in 32 bit chunks and 32 bit alinged
    // add all addresses in the range of [addr, addr + size) to data footprint
    // such that for 35-70 {32,64} is added
    cout << (UINT64)insAddr << " " << (UINT64)insAddr + insSize << endl;
    cout << "addr: ";
    for (UINT64 addr = (UINT64)insAddr / 32; addr < ((UINT64)insAddr + insSize) / 32 + (((UINT64)insAddr + insSize) % 32 != 0); addr++)
    {
        cout << addr * 32 << " ";
    }
    cout << endl;
}

int main(int argc, char const *argv[])
{
    unordered_map<UINT32, UINT64> ump;

    ump[52]++;
    cout << "sizeof(ump): " << ump[52] << endl;

    UINT64 something = atoll(argv[1]);
    UINT32 insSize = atoll(argv[2]);
    AnalysisMetrics((void *)something, insSize);
    std::cout << "Hello World!" << std::endl;
    return 0;
}
