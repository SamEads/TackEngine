#include "luainc.h"
int refcount = 0;
int refBaseline = -1;
std::unordered_map<std::string, int> n;