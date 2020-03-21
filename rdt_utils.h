#include <cstdint>
#include <list>

// output macros
#define SENDER_INFO(format, ...) \
    fprintf(stdout, "[%.2fs][INFO][ sender ]", GetSimulationTime());\
    fprintf(stdout, format "\n", ##__VA_ARGS__);

#define SENDER_WARNING(format, ...) \
    fprintf(stdout, "[%.2fs][WARN][ sender ]", GetSimulationTime());\
    fprintf(stdout, format "\n", ##__VA_ARGS__);

#define SENDER_ERROR(format, ...) \
    fprintf(stderr, "[%.2fs][EROR][ sender ]", GetSimulationTime());\
    fprintf(stderr, format "\n", ##__VA_ARGS__);

#define RECEIVER_INFO(format, ...) \
    fprintf(stdout, "[%.2fs][INFO][receiver]", GetSimulationTime());\
    fprintf(stdout, format "\n", ##__VA_ARGS__);
    
#define RECEIVER_WARNING(format, ...) \
    fprintf(stdout, "[%.2fs][WARN][receiver]", GetSimulationTime());\
    fprintf(stdout, format "\n", ##__VA_ARGS__);

#define RECEIVER_ERROR(format, ...) \
    fprintf(stderr, "[%.2fs][EROR][receiver]", GetSimulationTime());\
    fprintf(stderr, format "\n", ##__VA_ARGS__);

// CRC16 checksum library
class CRC16 {
private:
    static const uint16_t crc16tab[];
public:
    static uint16_t calc(const char *buf, int len, uint16_t crc = 0);
    static bool check(const char *buf, int len);
};