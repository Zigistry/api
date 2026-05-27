#include <iostream>

#include "../../include/crow_all.h"
#include "../../include/libsql.h"
#include <expected>

#define GET_ROW_UL(A, B) ((unsigned int)libsql_row_value((A), (B)).ok.value.integer)
#define GET_ROW_BOOL(A, B) ((bool)libsql_row_value((A), (B)).ok.value.integer)

std::string get_row_text(libsql_row_t row, int col);
std::string adv_tokenizer(std::string s, char del, int index);
std::expected<crow::json::wvalue, std::string> special_parsing(std::string query);


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

const std::string search_programs_database_query = R"""(


            WITH filtered AS MATERIALIZED (
                SELECT r.id
                FROM repos r
                WHERE r.is_disabled = 0
                  AND EXISTS (SELECT 1 FROM programs p WHERE p.repo_id = r.id)
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

const std::string infinite_scroll_packages_query = R"""(
    

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
            INNER JOIN packages pkg ON r.id = pkg.repo_id
            LEFT JOIN programs prog ON r.id = prog.repo_id
            WHERE
                r.is_disabled = 0
            ORDER BY r.stargazer_count DESC, r.id ASC
            LIMIT ? OFFSET ?
        )
        SELECT
            rd.*,
            (
                SELECT COUNT(*)
                FROM repo_dependents
                WHERE repo_id = rd.id
            ) AS dependents_count
        FROM repo_data rd;

            
)""";

const std::string infinite_scroll_programs_query = R"""(


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
                LIMIT ? OFFSET ?
            )
            SELECT
                rd.*,
                (
                    SELECT COUNT(*)
                    FROM repo_dependents
                    WHERE repo_id = rd.id
                ) AS dependents_count
            FROM repo_data rd;        

            
    )""";
