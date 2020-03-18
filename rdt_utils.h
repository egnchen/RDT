#include <cstdint>

namespace utils {

class CRC16 {
private:
    static const uint16_t crc16tab[];
public:
    static uint16_t calc(const char *, int len);
};

}