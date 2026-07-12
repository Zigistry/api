#include "./helper_lib/helper_lib.h"

extern sqlite3* database_connection;

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

    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(database_connection, repo_query.c_str(), -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, repo_id, strlen(repo_id), SQLITE_TRANSIENT);

    int step_rc = sqlite3_step(stmt);
    bool row_empty = (step_rc == SQLITE_DONE);

    if (row_empty) {
        sqlite3_finalize(stmt);
        crow::json::wvalue error;
        error["error"] = "Repository not found";
        return crow::response(404, error);
    }

    response["id"] = get_row_text(stmt, 0);
    response["avatar_id"] = get_row_text(stmt, 1);
    response["owner"] = get_row_text(stmt, 2);
    response["platform"] = get_row_text(stmt, 3);
    response["description"] = get_row_text(stmt, 4);
    response["issues_count"] = GET_ROW_UL(stmt, 5);
    response["default_branch_name"] = get_row_text(stmt, 6);
    response["fork_count"] = GET_ROW_UL(stmt, 7);
    response["stargazer_count"] = GET_ROW_UL(stmt, 8);
    response["watchers_count"] = GET_ROW_UL(stmt, 9);
    response["pushed_at"] = get_row_text(stmt, 10);
    response["created_at"] = get_row_text(stmt, 11);
    response["is_archived"] = GET_ROW_UL(stmt, 12) != 0;
    response["is_disabled"] = GET_ROW_UL(stmt, 13) != 0;
    response["is_fork"] = GET_ROW_UL(stmt, 14) != 0;
    response["license"] = get_row_text(stmt, 15);
    response["primary_language"] = get_row_text(stmt, 16);

    std::string id = get_row_text(stmt, 0);

    std::stringstream ss(id);
    std::string provider;
    std::string owner;
    std::string repo;

    if (std::getline(ss, provider, '/') && std::getline(ss, owner, '/') && std::getline(ss, repo, '/')) {
        response["provider_id"] = provider;
        response["owner_name"] = owner;
        response["repo_name"] = repo;
    }

    sqlite3_finalize(stmt);

    {
        crow::json::wvalue::list releases;

        const std::string releases_query = R"(
            SELECT version
            FROM releases
            WHERE repo_id = ?
            ORDER BY published_at DESC
        )";

        sqlite3_stmt* stmt2 = nullptr;
        sqlite3_prepare_v2(database_connection, releases_query.c_str(), -1, &stmt2, nullptr);
        sqlite3_bind_text(stmt2, 1, repo_id, strlen(repo_id), SQLITE_TRANSIENT);

        while (true) {
            int r = sqlite3_step(stmt2);
            if (r == SQLITE_DONE)
                break;
            if (r != SQLITE_ROW)
                break;
            releases.push_back(get_row_text(stmt2, 0));
        }

        sqlite3_finalize(stmt2);
        response["releases"] = std::move(releases);
    }

    {
        crow::json::wvalue::list dependents;

        const std::string dependents_query = R"(
            SELECT dependent
            FROM repo_dependents
            WHERE repo_id = ?
        )";

        sqlite3_stmt* stmt3 = nullptr;
        sqlite3_prepare_v2(database_connection, dependents_query.c_str(), -1, &stmt3, nullptr);
        sqlite3_bind_text(stmt3, 1, repo_id, strlen(repo_id), SQLITE_TRANSIENT);

        while (true) {
            int r = sqlite3_step(stmt3);
            if (r == SQLITE_DONE)
                break;
            if (r != SQLITE_ROW)
                break;
            dependents.push_back(get_row_text(stmt3, 0));
        }

        sqlite3_finalize(stmt3);
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

    sqlite3_stmt* stmt4 = nullptr;
    sqlite3_prepare_v2(database_connection, release_query.c_str(), -1, &stmt4, nullptr);
    sqlite3_bind_text(stmt4, 1, repo_id, strlen(repo_id), SQLITE_TRANSIENT);

    if (has_version) {
        sqlite3_bind_text(stmt4, 2, version, strlen(version), SQLITE_TRANSIENT);
    }

    int release_step = sqlite3_step(stmt4);
    bool release_row_empty = (release_step == SQLITE_DONE);

    if (!release_row_empty) {

        long release_id = GET_ROW_UL(stmt4, 0);

        response["version"] = has_version ? get_row_text(stmt4, 1) : "0.0.0";

        response["latest_version"] = has_version ? "" : get_row_text(stmt4, 1);

        response["published_at"] = get_row_text(stmt4, 2);

        auto min_zig = get_row_text(stmt4, 3);

        response["minimum_zig_version"] = min_zig.empty() ? "0.0.0" : min_zig;

        response["readme_url"] = get_row_text(stmt4, 4);
        response["is_prerelease"] = GET_ROW_UL(stmt4, 5) != 0;
        response["directory_files"] = get_row_text(stmt4, 6);

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

        sqlite3_stmt* stmt5 = nullptr;
        sqlite3_prepare_v2(database_connection, dep_query.c_str(), -1, &stmt5, nullptr);
        sqlite3_bind_int64(stmt5, 1, release_id);

        while (true) {
            int r = sqlite3_step(stmt5);
            if (r == SQLITE_DONE)
                break;
            if (r != SQLITE_ROW)
                break;
            crow::json::wvalue dep;

            dep["name"] = get_row_text(stmt5, 0);
            dep["url"] = get_row_text(stmt5, 1);
            dep["hash"] = get_row_text(stmt5, 2);
            dep["lazy"] = GET_ROW_UL(stmt5, 3) != 0;
            dep["path"] = get_row_text(stmt5, 4);

            deps.push_back(std::move(dep));
        }

        sqlite3_finalize(stmt5);
        response["dependencies"] = std::move(deps);
    }

    sqlite3_finalize(stmt4);

    return crow::response(response);
}
