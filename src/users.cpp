#include "../include/crow_all.h"
#include <sqlite3.h>
#include "./helper_lib/helper_lib.h"

extern sqlite3* database_connection;

crow::response get_user_route(const crow::request& req)
{
    std::lock_guard<std::mutex> lock(db_mutex);
    const char* user_id = req.url_params.get("q");
    std::string user_id_string;

    if (user_id == NULL) {
        crow::json::wvalue error_responce;
        error_responce["error"] = "Problem with server. 1";
        return crow::response(error_responce);
    }

    user_id_string = user_id;

    sqlite3_stmt* fetch_user_stmt = nullptr;
    int rc = sqlite3_prepare_v2(
        database_connection,
        "SELECT id, avatar_id, platform, bio FROM users WHERE id = ?",
        -1,
        &fetch_user_stmt,
        nullptr);

    if (rc != SQLITE_OK) {
        std::cout << sqlite3_errstr(rc) << std::endl;
        sqlite3_finalize(fetch_user_stmt);
        crow::json::wvalue error_responce;
        error_responce["error"] = "Problem with server. 1";
        return crow::response(error_responce);
    }

    int bind_rc = sqlite3_bind_text(
        fetch_user_stmt,
        1,
        user_id_string.c_str(),
        user_id_string.length(),
        SQLITE_TRANSIENT);

    if (bind_rc != SQLITE_OK) {
        std::cout << sqlite3_errstr(bind_rc) << std::endl;
        sqlite3_finalize(fetch_user_stmt);
        crow::json::wvalue error_responce;
        error_responce["error"] = "Problem with server. 1";
        return crow::response(error_responce);
    }

    int step_rc = sqlite3_step(fetch_user_stmt);
    if (step_rc == SQLITE_DONE) {
        sqlite3_finalize(fetch_user_stmt);
        crow::json::wvalue error_responce;
        error_responce["error"] = "Unknown id.";
        return crow::response(error_responce);
    }
    if (step_rc != SQLITE_ROW) {
        sqlite3_finalize(fetch_user_stmt);
        crow::json::wvalue error_responce;
        error_responce["error"] = "Unknown id.";
        return crow::response(error_responce);
    }

    crow::json::wvalue user_data;

    user_data["id"] = get_row_text(fetch_user_stmt, 0);
    user_data["avatar_id"] = get_row_text(fetch_user_stmt, 1);
    user_data["platform"] = get_row_text(fetch_user_stmt, 2);
    user_data["bio"] = get_row_text(fetch_user_stmt, 3);

    const std::string fetch_user_repos_query = R"""(

        SELECT
            r.id,
            u.avatar_id,
            r.owner,
            r.platform,
            r.description,
            r.issues_count,
            r.default_branch_name,
            r.fork_count,
            r.stargazer_count,
            r.watchers_count,
            r.pushed_at,
            r.created_at,
            r.is_archived,
            r.is_disabled,
            r.is_fork,
            r.license,
            r.primary_language,
            (
                SELECT minimum_zig_version
                FROM releases
                WHERE repo_id = r.id
                ORDER BY published_at DESC
                LIMIT 1
            ) AS minimum_zig_version,
            (
                SELECT COUNT(*)
                FROM repo_dependents
                WHERE repo_id = r.id
            ) AS dependents_count,
            pkg.repo_id AS is_package,
            prog.repo_id AS is_program
        FROM repos r
        LEFT JOIN users u ON r.owner = u.id
        LEFT JOIN packages pkg ON r.id = pkg.repo_id
        LEFT JOIN programs prog ON r.id = prog.repo_id
        WHERE r.owner = ? AND r.is_disabled = 0
        ORDER BY r.stargazer_count DESC;

    )""";

    sqlite3_stmt* fetch_user_repos_query_stmt = nullptr;
    int rc2 = sqlite3_prepare_v2(
        database_connection,
        fetch_user_repos_query.c_str(),
        -1,
        &fetch_user_repos_query_stmt,
        nullptr);

    if (rc2 != SQLITE_OK) {
        std::cout << sqlite3_errstr(rc2) << std::endl;
        sqlite3_finalize(fetch_user_repos_query_stmt);
        crow::json::wvalue error_response;
        error_response["error"] = "Problem with server. 2";
        return crow::response(error_response);
    }

    int bind_rc2 = sqlite3_bind_text(
        fetch_user_repos_query_stmt,
        1,
        user_id_string.c_str(),
        user_id_string.length(),
        SQLITE_TRANSIENT);

    if (bind_rc2 != SQLITE_OK) {
        std::cout << sqlite3_errstr(bind_rc2) << std::endl;
        sqlite3_finalize(fetch_user_repos_query_stmt);
        crow::json::wvalue error_response;
        error_response["error"] = "Problem with server. 3";
        return crow::response(error_response);
    }

    crow::json::wvalue::list packages;
    crow::json::wvalue::list programs;

    while (true) {
        int r = sqlite3_step(fetch_user_repos_query_stmt);
        if (r != SQLITE_ROW && r != SQLITE_DONE) {
            sqlite3_finalize(fetch_user_repos_query_stmt);
            crow::json::wvalue error_responce;
            error_responce["error"] = "Problem with server.";
            return crow::response(error_responce);
        }

        if (r == SQLITE_DONE) {
            break;
        }

        crow::json::wvalue item;
        std::string id = get_row_text(fetch_user_repos_query_stmt, 0);
        item["id"] = id;

        std::string provider = get_row_text(fetch_user_repos_query_stmt, 3);

        item["id"] = get_row_text(fetch_user_stmt, 0);
        item["avatar_url"] = get_row_text(fetch_user_repos_query_stmt, 1);
        item["owner_name"] = get_row_text(fetch_user_repos_query_stmt, 2);
        item["owner"] = get_row_text(fetch_user_repos_query_stmt, 2);

        item["repo_name"] = adv_tokenizer(id, '/', 2);
        item["provider"] = provider == "github" ? "gh" : "cb";

        item["description"] = get_row_text(fetch_user_repos_query_stmt, 4);
        item["platform"] = get_row_text(fetch_user_repos_query_stmt, 3);
        item["issues_count"] = GET_ROW_UL(fetch_user_repos_query_stmt, 5);
        item["default_branch_name"] = get_row_text(fetch_user_repos_query_stmt, 6);
        item["fork_count"] = GET_ROW_UL(fetch_user_repos_query_stmt, 7);
        item["stargazer_count"] = GET_ROW_UL(fetch_user_repos_query_stmt, 8);
        item["watchers_count"] = GET_ROW_UL(fetch_user_repos_query_stmt, 9);
        item["pushed_at"] = get_row_text(fetch_user_repos_query_stmt, 10);
        item["created_at"] = get_row_text(fetch_user_repos_query_stmt, 11);
        item["is_archived"] = GET_ROW_UL(fetch_user_repos_query_stmt, 12);
        item["is_disabled"] = GET_ROW_UL(fetch_user_repos_query_stmt, 13);
        item["is_fork"] = GET_ROW_UL(fetch_user_repos_query_stmt, 14);
        item["license"] = get_row_text(fetch_user_repos_query_stmt, 15);
        item["primary_language"] = get_row_text(fetch_user_repos_query_stmt, 16);
        item["minimum_zig_version"] = get_row_text(fetch_user_repos_query_stmt, 17) == "" ? "0.0.0" : get_row_text(fetch_user_repos_query_stmt, 17);

        item["dependents_count"] = GET_ROW_UL(fetch_user_repos_query_stmt, 18);
        const bool is_package = GET_ROW_BOOL(fetch_user_repos_query_stmt, 19);
        const bool is_program = GET_ROW_BOOL(fetch_user_repos_query_stmt, 20);

        if (is_package) {
            packages.push_back(item);
        } else if (is_program) {
            programs.push_back(item);
        }
    }
    sqlite3_finalize(fetch_user_repos_query_stmt);
    sqlite3_finalize(fetch_user_stmt);
    user_data["packages"] = std::move(packages);
    user_data["programs"] = std::move(programs);
    return crow::response(user_data);
}
