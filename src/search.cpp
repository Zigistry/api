#include "./helper_lib/helper_lib.h"

extern sqlite3* database_connection;

crow::response search(const crow::request& req, const std::string query_str)
{
    std::lock_guard<std::mutex> lock(db_mutex);
    auto start = std::chrono::steady_clock::now();
    const char* raw_search_query = req.url_params.get("q");
    const char* raw_page = req.url_params.get("page");
    const char* raw_per_page = req.url_params.get("per_page");

    if (raw_search_query == NULL or raw_page == NULL or raw_per_page == NULL) {
        crow::json::wvalue error_responce;
        error_responce["error"] = "Parameters needed: q, page and per_page.";
        return crow::response(error_responce);
    }

    std::string search_query;
    unsigned long page;
    unsigned long per_page;
    try {
        search_query = raw_search_query;
        page = std::max(std::stoul(raw_page), 1ul);
        per_page = std::max(std::stoul(raw_per_page), 1ul);

        per_page = std::min(per_page, 12UL);
        page = std::min(page, 1000000UL);
    } catch (...) {
        crow::json::wvalue error_responce;
        error_responce["error"] = "Parameters are not in the correct format";
        return crow::response(error_responce);
    }

    if (search_query.size() > 200) {
        crow::json::wvalue error_responce;
        error_responce["error"] = "Search query length can't be greater than 200";
        return crow::response(error_responce);
    }

    search_query += "*";
    const unsigned long offset = (page - 1) * per_page;

    sqlite3_stmt* query_stmt = nullptr;
    int rc = sqlite3_prepare_v2(database_connection, query_str.c_str(), -1, &query_stmt, nullptr);

    if (rc != SQLITE_OK) {
        std::cout << sqlite3_errstr(rc) << std::endl;
        sqlite3_finalize(query_stmt);
        crow::json::wvalue error_responce;
        error_responce["error"] = "Problem with server. 1";
        return crow::response(error_responce);
    }

    int bind_rc;
    bind_rc = sqlite3_bind_text(query_stmt, 1, search_query.c_str(), search_query.length(), SQLITE_TRANSIENT);
    if (bind_rc != SQLITE_OK) {
        std::cout << sqlite3_errstr(bind_rc) << std::endl;
        sqlite3_finalize(query_stmt);
        crow::json::wvalue error_responce;
        error_responce["error"] = "Problem with server. 1";
        return crow::response(error_responce);
    }

    bind_rc = sqlite3_bind_int64(query_stmt, 2, (sqlite3_int64)per_page);
    if (bind_rc != SQLITE_OK) {
        std::cout << sqlite3_errstr(bind_rc) << std::endl;
        sqlite3_finalize(query_stmt);
        crow::json::wvalue error_responce;
        error_responce["error"] = "Problem with server. 1";
        return crow::response(error_responce);
    }

    bind_rc = sqlite3_bind_int64(query_stmt, 3, (sqlite3_int64)offset);
    if (bind_rc != SQLITE_OK) {
        std::cout << sqlite3_errstr(bind_rc) << std::endl;
        sqlite3_finalize(query_stmt);
        crow::json::wvalue error_responce;
        error_responce["error"] = "Problem with server. 1";
        return crow::response(error_responce);
    }

    crow::json::wvalue normal_responce;

    unsigned int total_results = 0;
    bool if_read = false;

    crow::json::wvalue::list items;

    while (true) {
        int r = sqlite3_step(query_stmt);
        if (r != SQLITE_ROW && r != SQLITE_DONE) {
            sqlite3_finalize(query_stmt);
            crow::json::wvalue error_responce;
            error_responce["error"] = "Problem with server.";
            return crow::response(error_responce);
        }

        if (r == SQLITE_DONE) {
            break;
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
        if (!if_read) {
            total_results = GET_ROW_UL(query_stmt, 19);
            if_read = true;
        }
    }

    sqlite3_finalize(query_stmt);

    auto end = std::chrono::steady_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

    normal_responce["items"] = std::move(items);

    normal_responce["total"] = total_results;
    normal_responce["page"] = page;
    normal_responce["per_page"] = per_page;
    normal_responce["total_pages"] = std::ceil((float)total_results / per_page);
    normal_responce["time_took_to_search_ns"] = duration.count();

    return crow::response(normal_responce);
}
