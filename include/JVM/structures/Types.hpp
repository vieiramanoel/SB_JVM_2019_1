#ifndef _Types_H_
#define _Types_H_

#include <map>
#include <memory>
#include <string>

enum Type { B, C, D, F, I, J, S, Z, L, R };

static const std::map<std::string, Type> TypeMap = std::map<std::string, Type>{
    {"B", B}, {"C", C}, {"D", D}, {"F", F}, {"I", I},
    {"J", J}, {"S", S}, {"Z", Z}, {"L", L}};

int static category(Type t) {
    if (t == D || t == J) {
        return 2;
    }
    return 1;
}

static const std::map<int, Type> ATypeMap = std::map<int, Type>{
    {4, B}, {5, C}, {6, F}, {7, D}, {8, B}, {9, S}, {10, I}, {11, J},
};
/*B = byte
  C = char
  D = double
  F = float
  I = int
  J = long
  S = short
  Z = bool
  L = class
  R = string
*/

#endif