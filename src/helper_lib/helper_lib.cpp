#include <sstream>
#include <sqlite3.h>
#include <expected>
#include "../../include/crow_all.h"
#include "./helper_lib.h"
extern sqlite3* database_connection;

std::string get_row_text(sqlite3_stmt* stmt, int col)
{
    if (sqlite3_column_type(stmt, col) != SQLITE_TEXT)
        return "";
    int len = sqlite3_column_bytes(stmt, col);
    if (len <= 0)
        return "";
    return std::string((const char*)sqlite3_column_text(stmt, col), len);
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
    std::lock_guard<std::mutex> lock(db_mutex);

    sqlite3_stmt* query_stmt = nullptr;
    int rc = sqlite3_prepare_v2(database_connection, query.c_str(), -1, &query_stmt, nullptr);
    if (rc != SQLITE_OK) {
        return std::unexpected("Unable to parse the sql.");
    }

    crow::json::wvalue normal_responce;
    crow::json::wvalue::list items;

    while (true) {
        int step_rc = sqlite3_step(query_stmt);
        if (step_rc == SQLITE_DONE) {
            break;
        } else if (step_rc != SQLITE_ROW) {
            sqlite3_finalize(query_stmt);
            return std::unexpected("some problem with row and sql");
        }

        crow::json::wvalue item;
        std::string id = get_row_text(query_stmt, 0);
        item["id"] = id;

        std::string provider = get_row_text(query_stmt, 3);

        item["id"] = get_row_text(query_stmt, 0);
        item["avatar_url"] = get_row_text(query_stmt, 1);
        item["owner_name"] = get_row_text(query_stmt, 2);
        item["owner"] = get_row_text(query_stmt, 2);

        item["repo_name"] = adv_tokenizer(id, '/', 2);
        item["provider"] = provider == "github" ? "gh" : "cb";

        item["description"] = get_row_text(query_stmt, 4);
        item["platform"] = get_row_text(query_stmt, 3);
        item["issues_count"] = GET_ROW_UL(query_stmt, 5);
        item["default_branch_name"] = get_row_text(query_stmt, 6);
        item["fork_count"] = GET_ROW_UL(query_stmt, 7);
        item["stargazer_count"] = GET_ROW_UL(query_stmt, 8);
        item["watchers_count"] = GET_ROW_UL(query_stmt, 9);
        item["pushed_at"] = get_row_text(query_stmt, 10);
        item["created_at"] = get_row_text(query_stmt, 11);
        item["is_archived"] = GET_ROW_UL(query_stmt, 12);
        item["is_disabled"] = GET_ROW_UL(query_stmt, 13);
        item["is_fork"] = GET_ROW_UL(query_stmt, 14);
        item["license"] = get_row_text(query_stmt, 15);
        item["primary_language"] = get_row_text(query_stmt, 16);
        item["minimum_zig_version"] = get_row_text(query_stmt, 17) == "" ? "0.0.0" : get_row_text(query_stmt, 17);

        item["dependents_count"] = GET_ROW_UL(query_stmt, 18);

        items.push_back(item);
    }

    sqlite3_finalize(query_stmt);
    crow::json::wvalue res;
    res = std::move(items);
    return res;
}
