//
// Created by tate on 23-05-20.
//

#ifndef DLANG_GLOBAL_IDS_HPP
#define DLANG_GLOBAL_IDS_HPP

#include <cinttypes>
#include "gc/handle.hpp"

class Value;
const Value& get_global_id(int64_t id);
extern unsigned short global_ids_count;
#endif //DLANG_GLOBAL_IDS_HPP
