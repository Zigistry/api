#include "../include/crow_all.h"
#include "../include/libsql.h"
#include "./helper_lib/helper_lib.h"

extern libsql_connection_t database_connection;

crow::response get_user_route(const crow::request& req, const std::string query_str)
{
    const char* user_id = req.url_params.get("q");
    std::string user_id_string;

    if (user_id == NULL) {
        crow::json::wvalue error_responce;
        error_responce["error"] = "Problem with server. 1";
        return crow::response(error_responce);
    }

    user_id_string = user_id;

    const libsql_statement_t fetch_user_stmt = libsql_connection_prepare(
        database_connection,
        "SELECT id, avatar_id, platform, bio FROM users WHERE id = ?");

    if (fetch_user_stmt.err) {
        std::cout << libsql_error_message(fetch_user_stmt.err) << std::endl;
        crow::json::wvalue error_responce;
        error_responce["error"] = "Problem with server. 1";
        return crow::response(error_responce);
    }

    libsql_statement_bind_value(
        fetch_user_stmt,
        libsql_text(
            user_id_string.c_str(),
            user_id_string.length()));

    if (fetch_user_stmt.err) {
        std::cout << libsql_error_message(fetch_user_stmt.err) << std::endl;
        crow::json::wvalue error_responce;
        error_responce["error"] = "Problem with server. 1";
        return crow::response(error_responce);
    }
    const libsql_rows_t rows = libsql_statement_query(fetch_user_stmt);

    libsql_row_t row;
    if ((row = libsql_rows_next(rows)).err) {
        crow::json::wvalue error_responce;
        error_responce["error"] = "Unknown id.";
        return crow::response(error_responce);
    }

    if (libsql_row_empty(row)) {
        crow::json::wvalue error_responce;
        error_responce["error"] = "Unknown id.";
        return crow::response(error_responce);
    }

    crow::json::wvalue user_data;

    user_data["id"] = get_row_text(row, 0);
    user_data["avatar_id"] = get_row_text(row, 1);
    user_data["platform"] = get_row_text(row, 2);
    user_data["bio"] = get_row_text(row, 3);

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

    const libsql_rows_t rows_2 = libsql_statement_query(fetch_user_stmt);

    crow::json::wvalue::list packages;
    crow::json::wvalue::list programs;

    while (true) {
        libsql_row_t row3;
        if ((row3 = libsql_rows_next(rows_2)).err) {
            crow::json::wvalue error_responce;
            error_responce["error"] = "Problem with server.";
            return crow::response(error_responce);
        }

        if (libsql_row_empty(row3)) {
            break;
        }

        crow::json::wvalue item;
        std::string id = get_row_text(row3, 0);
        item["id"] = id;

        std::string provider = get_row_text(row3, 3);

        item["id"] = get_row_text(row, 0);
        item["avatar_url"] = get_row_text(row3, 1);
        item["owner_name"] = get_row_text(row3, 2);
        item["owner"] = get_row_text(row3, 2);

        item["repo_name"] = adv_tokenizer(id, '/', 2);
        item["provider"] = provider == "github" ? "gh" : "cb";

        item["description"] = get_row_text(row3, 4);
        item["platform"] = get_row_text(row3, 3);
        item["issues_count"] = GET_ROW_UL(row3, 5);
        item["default_branch_name"] = get_row_text(row3, 6);
        item["fork_count"] = GET_ROW_UL(row3, 7);
        item["stargazer_count"] = GET_ROW_UL(row3, 8);
        item["watchers_count"] = GET_ROW_UL(row3, 9);
        item["pushed_at"] = get_row_text(row3, 10);
        item["created_at"] = get_row_text(row3, 11);
        item["is_archived"] = GET_ROW_UL(row3, 12);
        item["is_disabled"] = GET_ROW_UL(row3, 13);
        item["is_fork"] = GET_ROW_UL(row3, 14);
        item["license"] = get_row_text(row3, 15);
        item["primary_language"] = get_row_text(row3, 16);
        item["minimum_zig_version"] = get_row_text(row3, 17) == "" ? "0.0.0" : get_row_text(row3, 17);

        item["dependents_count"] = GET_ROW_UL(row3, 18);
        const bool is_package = GET_ROW_UL(row3, 19);
        const bool is_program = GET_ROW_UL(row3, 20);

        if (is_package) {
            packages.push_back(item);
        } else if (is_program) {
            programs.push_back(item);
        }
    }
    user_data["packages"] = packages;
    user_data["programs"] = programs;
    return crow::response(user_data);
}
