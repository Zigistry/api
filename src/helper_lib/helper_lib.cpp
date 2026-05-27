#include <sstream>
#include "../../include/libsql.h"
#include <expected>
#include "../../include/crow_all.h"
#include "./helper_lib.h"
extern libsql_connection_t database_connection;

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


std::expected<crow::json::wvalue, std::string> special_parsing(std::string query)
{
    const libsql_statement_t query_stmt = libsql_connection_prepare(database_connection, query.c_str());

    if (query_stmt.err) {
        return std::unexpected("Unable to parse the sql.");
    }

    const libsql_rows_t rows = libsql_statement_query(query_stmt);

    if (rows.err) {
        return std::unexpected("Unable to parse the sql.");
    }

    crow::json::wvalue normal_responce;
    crow::json::wvalue::list items;

    while (true) {
        libsql_row_t row;
        if ((row = libsql_rows_next(rows)).err) {
            return std::unexpected("some problem with row and sql");
        }

        if (libsql_row_empty(row)) {
            break;
        }

        crow::json::wvalue item;
        std::string id = get_row_text(row, 0);
        item["id"] = id;

        std::string provider = get_row_text(row, 3);

        item["id"] = get_row_text(row, 0);
        item["avatar_url"] = get_row_text(row, 1);
        item["owner_name"] = get_row_text(row, 2);
        item["owner"] = get_row_text(row, 2);

        item["repo_name"] = adv_tokenizer(id, '/', 2);
        item["provider"] = provider == "github" ? "gh" : "cb";

        item["description"] = get_row_text(row, 4);
        item["platform"] = get_row_text(row, 3);
        item["issues_count"] = GET_ROW_UL(row, 5);
        item["default_branch_name"] = get_row_text(row, 6);
        item["fork_count"] = GET_ROW_UL(row, 7);
        item["stargazer_count"] = GET_ROW_UL(row, 8);
        item["watchers_count"] = GET_ROW_UL(row, 9);
        item["pushed_at"] = get_row_text(row, 10);
        item["created_at"] = get_row_text(row, 11);
        item["is_archived"] = GET_ROW_UL(row, 12);
        item["is_disabled"] = GET_ROW_UL(row, 13);
        item["is_fork"] = GET_ROW_UL(row, 14);
        item["license"] = get_row_text(row, 15);
        item["primary_language"] = get_row_text(row, 16);
        item["minimum_zig_version"] = get_row_text(row, 17) == "" ? "0.0.0" : get_row_text(row, 17);

        item["dependents_count"] = GET_ROW_UL(row, 18);

        items.push_back(item);
    }

    crow::json::wvalue res;
    res = std::move(items);
    return res;
}
