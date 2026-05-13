#include "./helper_lib/helper_lib.h"

extern libsql_connection_t database_connection;


crow::response search_packages(const crow::request& req)
{
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

    const std::string search_packages_database_query = R"""(


            WITH filtered AS MATERIALIZED (
                SELECT r.id
                FROM repos r
                WHERE r.is_disabled = 0
                  AND EXISTS (SELECT 1 FROM packages p WHERE p.repo_id = r.id)
                  AND (
                      r.id IN (SELECT repo_id FROM repo_search WHERE keywords MATCH ?)
                  )
            ),
            paged AS MATERIALIZED (
                SELECT id FROM filtered LIMIT ? OFFSET ?
            )
            SELECT
                r.id, u.avatar_id, r.owner, r.platform, r.description,
                r.issues_count, r.default_branch_name, r.fork_count,
                r.stargazer_count, r.watchers_count, r.pushed_at, r.created_at,
                r.is_archived, r.is_disabled, r.is_fork, r.license,
                r.primary_language,
                rel.minimum_zig_version,
                COALESCE(dep.dependents_count, 0) AS dependents_count,
                (SELECT COUNT(*) FROM filtered) AS total_results
            FROM paged
            JOIN repos r ON r.id = paged.id
            LEFT JOIN users u ON u.id = r.owner
            LEFT JOIN releases rel ON rel.repo_id = r.id AND rel.version = '__ZIGISTRY__DEFAULT__BRANCH__'
            LEFT JOIN (
                SELECT repo_id, COUNT(*) AS dependents_count
                FROM repo_dependents
                WHERE repo_id IN (SELECT id FROM paged)
                GROUP BY repo_id
            ) dep ON dep.repo_id = r.id;


    )""";

    const libsql_statement_t query_stmt = libsql_connection_prepare(database_connection, search_packages_database_query.c_str());

    if (query_stmt.err) {
        std::cout << libsql_error_message(query_stmt.err) << std::endl;
        crow::json::wvalue error_responce;
        error_responce["error"] = "Problem with server. 1";
        return crow::response(error_responce);
    }

    libsql_statement_bind_value(
        query_stmt,
        libsql_text(
            search_query.c_str(),
            search_query.length()));
    libsql_statement_bind_value(
        query_stmt,
        libsql_integer(
            per_page));

    libsql_statement_bind_value(
        query_stmt,
        libsql_integer(
            offset));

    if (query_stmt.err) {
        std::cout << libsql_error_message(query_stmt.err) << std::endl;
        crow::json::wvalue error_responce;
        error_responce["error"] = "Problem with server. 1";
        return crow::response(error_responce);
    }

    const libsql_rows_t rows = libsql_statement_query(query_stmt);

    if (rows.err) {
        crow::json::wvalue error_responce;
        error_responce["error"] = "Problem with server. 5";
        return crow::response(error_responce);
    }

    crow::json::wvalue normal_responce;

    unsigned int total_results = 0;
    bool if_read = false;

    crow::json::wvalue::list items;

    while (true) {
        libsql_row_t row;
        if ((row = libsql_rows_next(rows)).err) {
            crow::json::wvalue error_responce;
            error_responce["error"] = "Problem with server.";
            return crow::response(error_responce);
        }

        if (libsql_row_empty(row)) {
            break;
        }

        crow::json::wvalue item;

        item["id"] = get_row_text(row, 0);
        item["avatar_url"] = get_row_text(row, 1);
        item["owner_name"] = get_row_text(row, 2);
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
        item["minimum_zig_version"] = get_row_text(row, 17);
        item["dependents_count"] = GET_ROW_UL(row, 18);

        items.push_back(item);
        if (!if_read) {
            total_results = GET_ROW_UL(row, 19);
            if_read = true;
        }
    }

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


