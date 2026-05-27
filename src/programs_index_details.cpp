#include "../include/crow_all.h"
#include <sqlite3.h>
#include "./helper_lib/helper_lib.h"

extern sqlite3* database_connection;

crow::response programIndexDetails(const crow::request& req)
{
    const std::string get_latest_repos_query = R"""(


               WITH repo_data AS (
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
                        ) AS minimum_zig_version
                    FROM repos r
                    LEFT JOIN users u ON r.owner = u.id
                    LEFT JOIN packages pkg ON r.id = pkg.repo_id
                    LEFT JOIN programs prog ON r.id = prog.repo_id
                    WHERE
                        r.is_disabled = 0
                        AND prog.repo_id IS NOT NULL
                    ORDER BY r.created_at DESC
                    LIMIT 10
                )
                SELECT
                    rd.*,
                    (SELECT COUNT(*) FROM repo_dependents WHERE repo_id = rd.id) AS dependents_count
                FROM repo_data rd

    )""";

    // This is from the infinite scroll.
    const std::string get_most_used_repos_query = R"""(


        WITH repo_data AS (
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
                ) AS minimum_zig_version
        FROM repos r
        LEFT JOIN users u ON r.owner = u.id
        LEFT JOIN packages pkg ON r.id = pkg.repo_id
        LEFT JOIN programs prog ON r.id = prog.repo_id
        WHERE
            r.is_disabled = 0
            AND prog.repo_id IS NOT NULL
        ORDER BY r.stargazer_count DESC, r.id ASC
        LIMIT 10 OFFSET 0
        )
        SELECT
            rd.*,
            (SELECT COUNT(*) FROM repo_dependents WHERE repo_id = rd.id) AS dependents_count
        FROM repo_data rd

    )""";


    auto latest_repositories = special_parsing(get_latest_repos_query);
    auto most_used_repos = special_parsing(get_most_used_repos_query);

    if (latest_repositories and most_used_repos) {
        crow::json::wvalue normal_responce;

        normal_responce["latest"] = std::move(*latest_repositories);
        normal_responce["most_used"] = std::move(*most_used_repos);

        return crow::response(normal_responce);
    } else {
        crow::json::wvalue error_responce;
        error_responce["error"] = "some problem on server.";
        return crow::response(error_responce);
    }
}


