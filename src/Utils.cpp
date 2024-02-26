#include "Utils.hpp"

using namespace std;

const char scroll = 27;

void print_stats(map<string, int *> *stats)
{
    cout << scroll << "[2J\n";
    cout << scroll << "[1;1f";
    for (map<string, int *>::iterator stat = stats->begin(); stat != stats->end(); ++stat)
    {
        cout << stat->first << *(stat->second) << endl;
    }
    cout.flush();
}
