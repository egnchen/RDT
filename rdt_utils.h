#include <cstdint>
#include <list>

// debugger output
#define SENDER_INFO(format, ...) \
    fprintf(stdout, "[%.2fs][ sender ][INFO]", GetSimulationTime());\
    fprintf(stdout, format "\n", ##__VA_ARGS__);

#define SENDER_WARNING(format, ...) \
    fprintf(stderr, "[%.2fs][ sender ][WARN]", GetSimulationTime());\
    fprintf(stderr, format "\n", ##__VA_ARGS__);

#define SENDER_ERROR(format, ...) \
    fprintf(stderr, "[%.2fs][ sender ][EROR]", GetSimulationTime());\
    fprintf(stderr, format "\n", ##__VA_ARGS__);

#define RECEIVER_INFO(format, ...) \
    fprintf(stdout, "[%.2fs][receiver][INFO]", GetSimulationTime());\
    fprintf(stdout, format "\n", ##__VA_ARGS__);
    
#define RECEIVER_WARNING(format, ...) \
    fprintf(stdout, "[%.2fs][receiver][WARN]", GetSimulationTime());\
    fprintf(stdout, format "\n", ##__VA_ARGS__);

#define RECEIVER_ERROR(format, ...) \
    fprintf(stderr, "[%.2fs][receiver][EROR]", GetSimulationTime());\
    fprintf(stderr, format "\n", ##__VA_ARGS__);


class CRC16 {
private:
    static const uint16_t crc16tab[];
public:
    static uint16_t calc(const char *buf, int len, uint16_t crc = 0);
    static bool check(const char *buf, int len);
};