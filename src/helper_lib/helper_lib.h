#include <iostream>

#include "../../include/crow_all.h"
#include "../../include/libsql.h"

#define GET_ROW_UL(A, B) ((unsigned int)libsql_row_value((A), (B)).ok.value.integer)

inline std::string get_row_text(libsql_row_t row, int col)
{
    auto v = libsql_row_value(row, col);
    if (v.ok.type != LIBSQL_TYPE_TEXT)
        return "";
    int res = v.ok.value.text.len - 1;
    if (res <= 0)
        return "";
    return std::string((char*)v.ok.value.text.ptr, res);
}

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
        left join users u on r.owner = u.id
        left join packages pkg on r.id = pkg.repo_id
            left join programs prog on r.id = prog.repo_id
            where
                r.is_disabled = 0

        and pkg.repo_id is not null

        order by r.stargazer_count desc, r.id asc
            limit ? offset ?
        )
        select
            rd.*,
            (select count(*) from repo_dependents where repo_id = rd.id) as dependents_count
        from repo_data rd
            
)""";

