#include <algorithm>
#include <iostream>
#include <sstream>
#include <string.h>
#include <string>
#include <vector>

/**
 * Represents the constant pool entry of the type CONSTANT_Double_info (tag = 6)
 * more information at:
 * https://docs.oracle.com/javase/specs/jvms/se8/html/jvms-4.html#jvms-4.4.5
 */
struct Double {
    unsigned short int tag = 6;
    std::string high_bytes;
    std::string low_bytes;
    unsigned char full_bytes[8];
    double li;
    Double(std::vector<unsigned char> hbytes,
           std::vector<unsigned char> lbytes) {
        std::stringstream ss;
        for (auto b : hbytes) {
            ss << std::hex << static_cast<int>(b);
        }
        high_bytes = ss.str();
        ss         = std::stringstream();
        for (auto b : lbytes) {
            ss << std::hex << static_cast<int>(b);
        }
        low_bytes      = ss.str();
        unsigned int i = 0;
        for (; i < 4; i++) {
            full_bytes[7 - i] = hbytes[i];
        }
        for (; i < 8; i++) {
            full_bytes[7 - i] = lbytes[i - 4];
        }

        memcpy(&li, full_bytes, sizeof(double));
    };
    double getValue() { return li; };
};