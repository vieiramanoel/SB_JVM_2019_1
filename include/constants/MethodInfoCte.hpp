#ifndef _MethodInfoCte_H_
#define _MethodInfoCte_H_

#include <constants/AttributeCode.hpp>
#include <string>

struct MethodInfoCte {
    unsigned short int access_flags;
    unsigned short int name_index;
    std::string name;
    std::string descriptor;
    unsigned short int descriptor_index;
    int arg_length;
    unsigned short int attributes_count;
    std::vector<AttributeCode> attributes;
    std::vector<AttributeInfo> attributes_info;
    MethodInfoCte(int af, int ni, int di, int ac,
                  std::vector<AttributeCode> attrc,
                  std::vector<AttributeInfo> attri) {
        access_flags     = af;
        name_index       = ni;
        descriptor_index = di;
        attributes_count = ac;
        attributes       = attrc;
        attributes_info  = attri;
    }
};

#endif