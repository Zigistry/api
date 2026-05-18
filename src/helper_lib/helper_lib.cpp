#include <sstream>
#include "../../include/libsql.h"

std::string get_row_text(libsql_row_t row, int col)
{
    auto v = libsql_row_value(row, col);
    if (v.ok.type != LIBSQL_TYPE_TEXT)
        return "";
    int res = v.ok.value.text.len - 1;
    if (res <= 0)
        return "";
    return std::string((char*)v.ok.value.text.ptr, res);
}

std::string adv_tokenizer(std::string s, char del, int index)
{
    std::stringstream ss(s);
    std::string word;
    int count = -1;
    while (!ss.eof() and count++ != index) {
        getline(ss, word, del);
    }
    return word;
}

