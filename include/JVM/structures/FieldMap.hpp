#ifndef _FIELDMAP_H_
#define _FIELDMAP_H_

#include <JVM/structures/ContextEntry.hpp>
#include <map>
#include <string>

typedef std::map<int, std::shared_ptr<ContextEntry>> ClassFields;

#endif