#include "../include/crow_all.h"
#include "../include/libsql.h"
#include "./helper_lib/helper_lib.h"
#include <expected>
extern libsql_connection_t database_connection;


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

    normal_responce["latest"] = std::move(items);

    return normal_responce;
}



crow::response packageIndexDetails(const crow::request& req)
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
                        AND pkg.repo_id IS NOT NULL
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
            AND pkg.repo_id IS NOT NULL
        ORDER BY r.stargazer_count DESC, r.id ASC
        LIMIT 10 OFFSET 0
        )
        SELECT
            rd.*,
            (SELECT COUNT(*) FROM repo_dependents WHERE repo_id = rd.id) AS dependents_count
        FROM repo_data rd
            
    )""";

    std::string get_repo_gui_section = R"""(


        WITH section_repos AS (
            SELECT repo_id FROM index_sections WHERE section_name = 'gui'
        ),
        repo_data AS (
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
                JOIN section_repos sr ON r.id = sr.repo_id
                LEFT JOIN packages pkg ON r.id = pkg.repo_id
                LEFT JOIN programs prog ON r.id = prog.repo_id
                WHERE
                    r.is_disabled = 0
                    AND (pkg.repo_id IS NOT NULL OR prog.repo_id IS NOT NULL)
                LIMIT 10
            )
        SELECT
            rd.*,
            (SELECT COUNT(*) FROM repo_dependents WHERE repo_id = rd.id) AS dependents_count
        FROM repo_data rd

        
        )""";


            std::string get_repo_games_section = R"""(


        WITH section_repos AS (
            SELECT repo_id FROM index_sections WHERE section_name = 'games'
        ),
        repo_data AS (
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
                JOIN section_repos sr ON r.id = sr.repo_id
                LEFT JOIN packages pkg ON r.id = pkg.repo_id
                LEFT JOIN programs prog ON r.id = prog.repo_id
                WHERE
                    r.is_disabled = 0
                    AND (pkg.repo_id IS NOT NULL OR prog.repo_id IS NOT NULL)
                LIMIT 10
            )
        SELECT
            rd.*,
            (SELECT COUNT(*) FROM repo_dependents WHERE repo_id = rd.id) AS dependents_count
        FROM repo_data rd

        
        )""";

    std::string get_repo_games_section = R"""(


        WITH section_repos AS (
            SELECT repo_id FROM index_sections WHERE section_name = 'web'
        ),
        repo_data AS (
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
                JOIN section_repos sr ON r.id = sr.repo_id
                LEFT JOIN packages pkg ON r.id = pkg.repo_id
                LEFT JOIN programs prog ON r.id = prog.repo_id
                WHERE
                    r.is_disabled = 0
                    AND (pkg.repo_id IS NOT NULL OR prog.repo_id IS NOT NULL)
                LIMIT 10
            )
        SELECT
            rd.*,
            (SELECT COUNT(*) FROM repo_dependents WHERE repo_id = rd.id) AS dependents_count
        FROM repo_data rd

        
        )""";


    auto latest_repositories = special_parsing(get_latest_repos_query);
    auto most_used_repos = special_parsing(get_most_used_repos_query);
    auto games_repos = special_parsing(get_repo_games_sections);
    auto gui_repos = special_parsing(get_repo_gui_sections);
    auto web_repos = special_parsing(get_repo_web_sections);

    if(latest_repositories)

    
}

