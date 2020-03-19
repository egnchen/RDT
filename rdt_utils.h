#include <cstdint>
#include <list>

// define the routines provided by the simulator

/* get simulation time (in seconds) */
double GetSimulationTime();
void Sender_StartTimer(double timeout);
void Sender_StopTimer();
bool Sender_isTimerSet();

namespace utils {

class CRC16 {
private:
    static const uint16_t crc16tab[];
public:
    static uint16_t calc(const char *, int len, uint16_t crc = 0);
    static bool check(const char *, int len);
};
}