#include "./helper_lib/helper_lib.h"

extern libsql_connection_t database_connection;

crow::response package_details(const crow::request& req)
{
    std::lock_guard<std::mutex> lock(db_mutex);
    const char* repo_id = req.url_params.get("q");

    if (!repo_id) {
        crow::json::wvalue error;
        error["error"] = "Parameter q is required";
        return crow::response(400, error);
    }

    const char* version = req.url_params.get("version");
    const bool has_version = version != nullptr;

    crow::json::wvalue response;

    const std::string repo_query = R"(
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
            r.primary_language
        FROM repos r
        LEFT JOIN users u ON r.owner = u.id
        WHERE r.id = ?
        LIMIT 1
    )";

    auto stmt = libsql_connection_prepare(database_connection, repo_query.c_str());

    libsql_statement_bind_value(
        stmt,
        libsql_text(repo_id, strlen(repo_id)));

    auto rows = libsql_statement_query(stmt);
    auto row = libsql_rows_next(rows);

    if (libsql_row_empty(row)) {
        crow::json::wvalue error;
        error["error"] = "Repository not found";
        return crow::response(404, error);
    }

    response["id"] = get_row_text(row, 0);
    response["avatar_id"] = get_row_text(row, 1);
    response["owner"] = get_row_text(row, 2);
    response["platform"] = get_row_text(row, 3);
    response["description"] = get_row_text(row, 4);
    response["issues_count"] = GET_ROW_UL(row, 5);
    response["default_branch_name"] = get_row_text(row, 6);
    response["fork_count"] = GET_ROW_UL(row, 7);
    response["stargazer_count"] = GET_ROW_UL(row, 8);
    response["watchers_count"] = GET_ROW_UL(row, 9);
    response["pushed_at"] = get_row_text(row, 10);
    response["created_at"] = get_row_text(row, 11);
    response["is_archived"] = GET_ROW_UL(row, 12) != 0;
    response["is_disabled"] = GET_ROW_UL(row, 13) != 0;
    response["is_fork"] = GET_ROW_UL(row, 14) != 0;
    response["license"] = get_row_text(row, 15);
    response["primary_language"] = get_row_text(row, 16);

    std::string id = get_row_text(row, 0);

    std::stringstream ss(id);
    std::string provider;
    std::string owner;
    std::string repo;

    if (std::getline(ss, provider, '/') && std::getline(ss, owner, '/') && std::getline(ss, repo, '/')) {
        response["provider_id"] = provider;
        response["owner_name"] = owner;
        response["repo_name"] = repo;
    }

    {
        crow::json::wvalue::list releases;

        const std::string releases_query = R"(
            SELECT version
            FROM releases
            WHERE repo_id = ?
            ORDER BY published_at DESC
        )";

        auto stmt2 = libsql_connection_prepare(
            database_connection,
            releases_query.c_str());

        libsql_statement_bind_value(
            stmt2,
            libsql_text(repo_id, strlen(repo_id)));

        auto rows2 = libsql_statement_query(stmt2);

        for (auto r = libsql_rows_next(rows2);
            !libsql_row_empty(r);
            r = libsql_rows_next(rows2)) {
            releases.push_back(get_row_text(r, 0));
        }

        response["releases"] = std::move(releases);
    }

    {
        crow::json::wvalue::list dependents;

        const std::string dependents_query = R"(
            SELECT dependent
            FROM repo_dependents
            WHERE repo_id = ?
        )";

        auto stmt3 = libsql_connection_prepare(
            database_connection,
            dependents_query.c_str());

        libsql_statement_bind_value(
            stmt3,
            libsql_text(repo_id, strlen(repo_id)));

        auto rows3 = libsql_statement_query(stmt3);

        for (auto r = libsql_rows_next(rows3);
            !libsql_row_empty(r);
            r = libsql_rows_next(rows3)) {
            dependents.push_back(get_row_text(r, 0));
        }

        response["dependents"] = std::move(dependents);
    }

    std::string release_query;

    if (has_version) {
        release_query = R"(
            SELECT
                id,
                version,
                published_at,
                minimum_zig_version,
                readme_url,
                is_prerelease,
                directory_files
            FROM releases
            WHERE repo_id = ?
            AND version = ?
            LIMIT 1
        )";
    } else {
        release_query = R"(
            SELECT
                id,
                version,
                published_at,
                minimum_zig_version,
                readme_url,
                is_prerelease,
                directory_files
            FROM releases
            WHERE repo_id = ?
            ORDER BY published_at DESC
            LIMIT 1
        )";
    }

    auto stmt4 = libsql_connection_prepare(
        database_connection,
        release_query.c_str());

    libsql_statement_bind_value(
        stmt4,
        libsql_text(repo_id, strlen(repo_id)));

    if (has_version) {
        libsql_statement_bind_value(
            stmt4,
            libsql_text(version, strlen(version)));
    }

    auto rows4 = libsql_statement_query(stmt4);
    auto release_row = libsql_rows_next(rows4);

    if (!libsql_row_empty(release_row)) {

        long release_id = GET_ROW_UL(release_row, 0);

        response["version"] = has_version ? get_row_text(release_row, 1) : "0.0.0";

        response["latest_version"] = has_version ? "" : get_row_text(release_row, 1);

        response["published_at"] = get_row_text(release_row, 2);

        auto min_zig = get_row_text(release_row, 3);

        response["minimum_zig_version"] = min_zig.empty() ? "0.0.0" : min_zig;

        response["readme_url"] = get_row_text(release_row, 4);
        response["is_prerelease"] = GET_ROW_UL(release_row, 5) != 0;
        response["directory_files"] = get_row_text(release_row, 6);

        crow::json::wvalue::list deps;

        const std::string dep_query = R"(
            SELECT
                name,
                url,
                hash,
                is_lazy,
                path
            FROM release_dependencies
            WHERE release_id = ?
        )";

        auto stmt5 = libsql_connection_prepare(
            database_connection,
            dep_query.c_str());

        libsql_statement_bind_value(
            stmt5,
            libsql_integer(release_id));

        auto rows5 = libsql_statement_query(stmt5);

        for (auto dep_row = libsql_rows_next(rows5);
            !libsql_row_empty(dep_row);
            dep_row = libsql_rows_next(rows5)) {
            crow::json::wvalue dep;

            dep["name"] = get_row_text(dep_row, 0);
            dep["url"] = get_row_text(dep_row, 1);
            dep["hash"] = get_row_text(dep_row, 2);
            dep["lazy"] = GET_ROW_UL(dep_row, 3) != 0;
            dep["path"] = get_row_text(dep_row, 4);

            deps.push_back(std::move(dep));
        }

        response["dependencies"] = std::move(deps);
    }

    return crow::response(response);
}
