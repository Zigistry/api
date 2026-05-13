#include <iostream>

#include "../../include/crow_all.h"
#include "../../include/libsql.h"


#define GET_ROW_UL(A, B) ((unsigned int)libsql_row_value((A), (B)).ok.value.integer)

inline std::string get_row_text(libsql_row_t row, int col)
{
    auto v = libsql_row_value(row, col);
    if (v.ok.type != LIBSQL_TYPE_TEXT)
        return "";
    int res = v.ok.value.text.len - 1;
    if (res <= 0)
        return "";
    return std::string((char*)v.ok.value.text.ptr, res);
}
