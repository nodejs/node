/* IMPORTANT
 * This snapshot file is auto-generated, but designed for humans.
 * It should be checked into source control and tracked carefully.
 * Re-generate by setting TAP_SNAPSHOT=1 and running tests.
 * Make sure to inspect the output below.  Do not ignore changes!
 */
'use strict'
exports[`test/lib/ls.js TAP ls --depth=0 > should output tree containing only top-level dependencies 1`] = `
test-npm-ls@1.0.0 {CWD}/ls-ls--depth-0
+-- foo@1.0.0
\`-- lorem@1.0.0

`

exports[`test/lib/ls.js TAP ls --depth=1 > should output tree containing top-level deps and their deps only 1`] = `
test-npm-ls@1.0.0 {CWD}/ls-ls--depth-1
+-- a@1.0.0
| \`-- b@1.0.0
\`-- e@1.0.0

`

exports[`test/lib/ls.js TAP ls --dev > should output tree containing dev deps 1`] = `
test-npm-ls@1.0.0 {CWD}/ls-ls--dev
\`-- dev-dep@1.0.0
  \`-- foo@1.0.0
    \`-- bar@1.0.0

`

exports[`test/lib/ls.js TAP ls --link > should output tree containing linked deps 1`] = `
test-npm-ls@1.0.0 {CWD}/ls-ls--link
\`-- linked-dep@1.0.0 -> {CWD}/ls-ls--link/linked-dep

`

exports[`test/lib/ls.js TAP ls --long --depth=0 > should output tree containing top-level deps with descriptions 1`] = `
test-npm-ls@1.0.0
| {CWD}/ls-ls--long-depth-0
| 
+-- dev-dep@1.0.0
|   A DEV dep kind of dep
+-- lorem@1.0.0
|   
+-- optional-dep@1.0.0
|   Maybe a dep?
+-- peer-dep@1.0.0
|   Peer-dep description here
\`-- prod-dep@1.0.0
    A PROD dep kind of dep

`

exports[`test/lib/ls.js TAP ls --long > should output tree info with descriptions 1`] = `
test-npm-ls@1.0.0
| {CWD}/ls-ls--long
| 
+-- dev-dep@1.0.0
| | A DEV dep kind of dep
| \`-- foo@1.0.0
|   | 
|   \`-- bar@1.0.0
|       
+-- lorem@1.0.0
|   
+-- optional-dep@1.0.0
|   Maybe a dep?
+-- peer-dep@1.0.0
|   Peer-dep description here
\`-- prod-dep@1.0.0
  | A PROD dep kind of dep
  \`-- bar@2.0.0
      A dep that bars

`

exports[`test/lib/ls.js TAP ls --only=development > should output tree containing only development deps 1`] = `
test-npm-ls@1.0.0 {CWD}/ls-ls--only-development
\`-- dev-dep@1.0.0
  \`-- foo@1.0.0
    \`-- bar@1.0.0

`

exports[`test/lib/ls.js TAP ls --only=prod > should output tree containing only prod deps 1`] = `
test-npm-ls@1.0.0 {CWD}/ls-ls--only-prod
+-- lorem@1.0.0
+-- optional-dep@1.0.0
\`-- prod-dep@1.0.0
  \`-- bar@2.0.0

`

exports[`test/lib/ls.js TAP ls --parseable --depth=0 > should output tree containing only top-level dependencies 1`] = `
{CWD}/ls-ls-parseable--depth-0
{CWD}/ls-ls-parseable--depth-0/node_modules/foo
{CWD}/ls-ls-parseable--depth-0/node_modules/lorem
`

exports[`test/lib/ls.js TAP ls --parseable --depth=1 > should output parseable containing top-level deps and their deps only 1`] = `
{CWD}/ls-ls-parseable--depth-1
{CWD}/ls-ls-parseable--depth-1/node_modules/foo
{CWD}/ls-ls-parseable--depth-1/node_modules/lorem
{CWD}/ls-ls-parseable--depth-1/node_modules/bar
`

exports[`test/lib/ls.js TAP ls --parseable --dev > should output tree containing dev deps 1`] = `
{CWD}/ls-ls-parseable--dev
{CWD}/ls-ls-parseable--dev/node_modules/dev-dep
{CWD}/ls-ls-parseable--dev/node_modules/foo
{CWD}/ls-ls-parseable--dev/node_modules/bar
`

exports[`test/lib/ls.js TAP ls --parseable --link > should output tree containing linked deps 1`] = `
{CWD}/ls-ls-parseable--link
{CWD}/ls-ls-parseable--link/node_modules/linked-dep
`

exports[`test/lib/ls.js TAP ls --parseable --long --depth=0 > should output tree containing top-level deps with descriptions 1`] = `
{CWD}/ls-ls-parseable--long-depth-0:test-npm-ls@1.0.0
{CWD}/ls-ls-parseable--long-depth-0/node_modules/dev-dep:dev-dep@1.0.0
{CWD}/ls-ls-parseable--long-depth-0/node_modules/lorem:lorem@1.0.0
{CWD}/ls-ls-parseable--long-depth-0/node_modules/optional-dep:optional-dep@1.0.0
{CWD}/ls-ls-parseable--long-depth-0/node_modules/peer-dep:peer-dep@1.0.0
{CWD}/ls-ls-parseable--long-depth-0/node_modules/prod-dep:prod-dep@1.0.0
`

exports[`test/lib/ls.js TAP ls --parseable --long > should output tree info with descriptions 1`] = `
{CWD}/ls-ls-parseable--long:test-npm-ls@1.0.0
{CWD}/ls-ls-parseable--long/node_modules/dev-dep:dev-dep@1.0.0
{CWD}/ls-ls-parseable--long/node_modules/lorem:lorem@1.0.0
{CWD}/ls-ls-parseable--long/node_modules/optional-dep:optional-dep@1.0.0
{CWD}/ls-ls-parseable--long/node_modules/peer-dep:peer-dep@1.0.0
{CWD}/ls-ls-parseable--long/node_modules/prod-dep:prod-dep@1.0.0
{CWD}/ls-ls-parseable--long/node_modules/foo:foo@1.0.0
{CWD}/ls-ls-parseable--long/node_modules/prod-dep/node_modules/bar:bar@2.0.0
{CWD}/ls-ls-parseable--long/node_modules/bar:bar@1.0.0
`

exports[`test/lib/ls.js TAP ls --parseable --long missing/invalid/extraneous > should output parseable result containing EXTRANEOUS/INVALID labels 1`] = `
{CWD}/ls-ls-parseable--long-missing-invalid-extraneous:test-npm-ls@1.0.0
{CWD}/ls-ls-parseable--long-missing-invalid-extraneous/node_modules/foo:foo@1.0.0:INVALID
{CWD}/ls-ls-parseable--long-missing-invalid-extraneous/node_modules/lorem:lorem@1.0.0:EXTRANEOUS
{CWD}/ls-ls-parseable--long-missing-invalid-extraneous/node_modules/bar:bar@1.0.0
`

exports[`test/lib/ls.js TAP ls --parseable --long print symlink target location > should output parseable results with symlink targets 1`] = `
{CWD}/ls-ls-parseable--long-print-symlink-target-location:test-npm-ls@1.0.0
{CWD}/ls-ls-parseable--long-print-symlink-target-location/node_modules/dev-dep:dev-dep@1.0.0
{CWD}/ls-ls-parseable--long-print-symlink-target-location/node_modules/linked-dep:linked-dep@1.0.0:{CWD}/ls-ls-parseable--long-print-symlink-target-location/linked-dep
{CWD}/ls-ls-parseable--long-print-symlink-target-location/node_modules/lorem:lorem@1.0.0
{CWD}/ls-ls-parseable--long-print-symlink-target-location/node_modules/optional-dep:optional-dep@1.0.0
{CWD}/ls-ls-parseable--long-print-symlink-target-location/node_modules/peer-dep:peer-dep@1.0.0
{CWD}/ls-ls-parseable--long-print-symlink-target-location/node_modules/prod-dep:prod-dep@1.0.0
{CWD}/ls-ls-parseable--long-print-symlink-target-location/node_modules/foo:foo@1.0.0
{CWD}/ls-ls-parseable--long-print-symlink-target-location/node_modules/prod-dep/node_modules/bar:bar@2.0.0
{CWD}/ls-ls-parseable--long-print-symlink-target-location/node_modules/bar:bar@1.0.0
`

exports[`test/lib/ls.js TAP ls --parseable --long with extraneous deps > should output long parseable output with extraneous info 1`] = `
{CWD}/ls-ls-parseable--long-with-extraneous-deps:test-npm-ls@1.0.0
{CWD}/ls-ls-parseable--long-with-extraneous-deps/node_modules/foo:foo@1.0.0
{CWD}/ls-ls-parseable--long-with-extraneous-deps/node_modules/lorem:lorem@1.0.0:EXTRANEOUS
{CWD}/ls-ls-parseable--long-with-extraneous-deps/node_modules/bar:bar@1.0.0
`

exports[`test/lib/ls.js TAP ls --parseable --only=development > should output tree containing only development deps 1`] = `
{CWD}/ls-ls-parseable--only-development
{CWD}/ls-ls-parseable--only-development/node_modules/dev-dep
{CWD}/ls-ls-parseable--only-development/node_modules/foo
{CWD}/ls-ls-parseable--only-development/node_modules/bar
`

exports[`test/lib/ls.js TAP ls --parseable --only=prod > should output tree containing only prod deps 1`] = `
{CWD}/ls-ls-parseable--only-prod
{CWD}/ls-ls-parseable--only-prod/node_modules/lorem
{CWD}/ls-ls-parseable--only-prod/node_modules/optional-dep
{CWD}/ls-ls-parseable--only-prod/node_modules/prod-dep
{CWD}/ls-ls-parseable--only-prod/node_modules/prod-dep/node_modules/bar
`

exports[`test/lib/ls.js TAP ls --parseable --production > should output tree containing production deps 1`] = `
{CWD}/ls-ls-parseable--production
{CWD}/ls-ls-parseable--production/node_modules/lorem
{CWD}/ls-ls-parseable--production/node_modules/optional-dep
{CWD}/ls-ls-parseable--production/node_modules/prod-dep
{CWD}/ls-ls-parseable--production/node_modules/prod-dep/node_modules/bar
`

exports[`test/lib/ls.js TAP ls --parseable cycle deps > should print tree output omitting deduped ref 1`] = `
{CWD}/ls-ls-parseable-cycle-deps
{CWD}/ls-ls-parseable-cycle-deps/node_modules/a
{CWD}/ls-ls-parseable-cycle-deps/node_modules/b
`

exports[`test/lib/ls.js TAP ls --parseable default --depth value should be 0 > should output parseable output containing only top-level dependencies 1`] = `
{CWD}/ls-ls-parseable-default-depth-value-should-be-0
{CWD}/ls-ls-parseable-default-depth-value-should-be-0/node_modules/foo
{CWD}/ls-ls-parseable-default-depth-value-should-be-0/node_modules/lorem
`

exports[`test/lib/ls.js TAP ls --parseable empty location > should print empty result 1`] = `
{CWD}/ls-ls-parseable-empty-location
`

exports[`test/lib/ls.js TAP ls --parseable extraneous deps > should output containing problems info 1`] = `
{CWD}/ls-ls-parseable-extraneous-deps
{CWD}/ls-ls-parseable-extraneous-deps/node_modules/foo
{CWD}/ls-ls-parseable-extraneous-deps/node_modules/lorem
{CWD}/ls-ls-parseable-extraneous-deps/node_modules/bar
`

exports[`test/lib/ls.js TAP ls --parseable from and resolved properties > should not be printed in tree output 1`] = `
{CWD}/ls-ls-parseable-from-and-resolved-properties
{CWD}/ls-ls-parseable-from-and-resolved-properties/node_modules/simple-output
`

exports[`test/lib/ls.js TAP ls --parseable global > should print parseable output for global deps 1`] = `
{CWD}/ls-ls-parseable-global
{CWD}/ls-ls-parseable-global/node_modules/a
{CWD}/ls-ls-parseable-global/node_modules/b
{CWD}/ls-ls-parseable-global/node_modules/b/node_modules/c
`

exports[`test/lib/ls.js TAP ls --parseable json read problems > should print empty result 1`] = `
{CWD}/ls-ls-parseable-json-read-problems
`

exports[`test/lib/ls.js TAP ls --parseable missing package.json > should log all extraneous deps on error msg 1`] = `
extraneous: bar@1.0.0 {CWD}/ls-ls-parseable-missing-package-json/node_modules/bar
extraneous: foo@1.0.0 {CWD}/ls-ls-parseable-missing-package-json/node_modules/foo
extraneous: lorem@1.0.0 {CWD}/ls-ls-parseable-missing-package-json/node_modules/lorem
`

exports[`test/lib/ls.js TAP ls --parseable missing package.json > should output parseable missing name/version of top-level package 1`] = `
{CWD}/ls-ls-parseable-missing-package-json
{CWD}/ls-ls-parseable-missing-package-json/node_modules/bar
{CWD}/ls-ls-parseable-missing-package-json/node_modules/foo
{CWD}/ls-ls-parseable-missing-package-json/node_modules/lorem
`

exports[`test/lib/ls.js TAP ls --parseable missing/invalid/extraneous > should output parseable containing top-level deps and their deps only 1`] = `
{CWD}/ls-ls-parseable-missing-invalid-extraneous
{CWD}/ls-ls-parseable-missing-invalid-extraneous/node_modules/foo
{CWD}/ls-ls-parseable-missing-invalid-extraneous/node_modules/lorem
{CWD}/ls-ls-parseable-missing-invalid-extraneous/node_modules/bar
`

exports[`test/lib/ls.js TAP ls --parseable no args > should output parseable representation of dependencies structure 1`] = `
{CWD}/ls-ls-parseable-no-args
{CWD}/ls-ls-parseable-no-args/node_modules/foo
{CWD}/ls-ls-parseable-no-args/node_modules/lorem
{CWD}/ls-ls-parseable-no-args/node_modules/bar
`

exports[`test/lib/ls.js TAP ls --parseable resolved points to git ref > should output tree containing git refs 1`] = `
{CWD}/ls-ls-parseable-resolved-points-to-git-ref
{CWD}/ls-ls-parseable-resolved-points-to-git-ref/node_modules/abbrev
`

exports[`test/lib/ls.js TAP ls --parseable unmet optional dep > should output parseable with empty entry for missing optional deps 1`] = `
{CWD}/ls-ls-parseable-unmet-optional-dep
{CWD}/ls-ls-parseable-unmet-optional-dep/node_modules/dev-dep
{CWD}/ls-ls-parseable-unmet-optional-dep/node_modules/lorem
{CWD}/ls-ls-parseable-unmet-optional-dep/node_modules/optional-dep
{CWD}/ls-ls-parseable-unmet-optional-dep/node_modules/peer-dep
{CWD}/ls-ls-parseable-unmet-optional-dep/node_modules/prod-dep
{CWD}/ls-ls-parseable-unmet-optional-dep/node_modules/foo
{CWD}/ls-ls-parseable-unmet-optional-dep/node_modules/prod-dep/node_modules/bar
{CWD}/ls-ls-parseable-unmet-optional-dep/node_modules/bar
`

exports[`test/lib/ls.js TAP ls --parseable unmet peer dep > should output parseable signaling missing peer dep in problems 1`] = `
{CWD}/ls-ls-parseable-unmet-peer-dep
{CWD}/ls-ls-parseable-unmet-peer-dep/node_modules/dev-dep
{CWD}/ls-ls-parseable-unmet-peer-dep/node_modules/lorem
{CWD}/ls-ls-parseable-unmet-peer-dep/node_modules/optional-dep
{CWD}/ls-ls-parseable-unmet-peer-dep/node_modules/peer-dep
{CWD}/ls-ls-parseable-unmet-peer-dep/node_modules/prod-dep
{CWD}/ls-ls-parseable-unmet-peer-dep/node_modules/foo
{CWD}/ls-ls-parseable-unmet-peer-dep/node_modules/prod-dep/node_modules/bar
{CWD}/ls-ls-parseable-unmet-peer-dep/node_modules/bar
`

exports[`test/lib/ls.js TAP ls --parseable using aliases > should output tree containing aliases 1`] = `
{CWD}/ls-ls-parseable-using-aliases
{CWD}/ls-ls-parseable-using-aliases/node_modules/a
`

exports[`test/lib/ls.js TAP ls --parseable with filter arg > should output parseable contaning only occurrences of filtered by package 1`] = `
{CWD}/ls-ls-parseable-with-filter-arg/node_modules/lorem
`

exports[`test/lib/ls.js TAP ls --parseable with filter arg nested dep > should output parseable contaning only occurrences of filtered package 1`] = `
{CWD}/ls-ls-parseable-with-filter-arg-nested-dep/node_modules/bar
`

exports[`test/lib/ls.js TAP ls --parseable with missing filter arg > should output parseable output containing no dependencies info 1`] = `

`

exports[`test/lib/ls.js TAP ls --parseable with multiple filter args > should output parseable contaning only occurrences of multiple filtered packages and their ancestors 1`] = `
{CWD}/ls-ls-parseable-with-multiple-filter-args/node_modules/lorem
{CWD}/ls-ls-parseable-with-multiple-filter-args/node_modules/bar
`

exports[`test/lib/ls.js TAP ls --production > should output tree containing production deps 1`] = `
test-npm-ls@1.0.0 {CWD}/ls-ls--production
+-- lorem@1.0.0
+-- optional-dep@1.0.0
\`-- prod-dep@1.0.0
  \`-- bar@2.0.0

`

exports[`test/lib/ls.js TAP ls broken resolved field > should NOT print git refs in output tree 1`] = `
npm-broken-resolved-field-test@1.0.0 {CWD}/ls-ls-broken-resolved-field
\`-- a@1.0.1

`

exports[`test/lib/ls.js TAP ls colored output > should output tree containing color info 1`] = `
[0mtest-npm-ls@1.0.0 {CWD}/ls-ls-colored-output[0m
[0m+-- foo@1.0.0 [31m[40minvalid[49m[39m[0m
[0m| \`-- bar@1.0.0[0m
[0m+-- [31m[40mUNMET DEPENDENCY[49m[39m ipsum@^1.0.0[0m
[0m\`-- lorem@1.0.0 [32m[40mextraneous[49m[39m[0m
[0m[0m
`

exports[`test/lib/ls.js TAP ls cycle deps > should print tree output containing deduped ref 1`] = `
test-npm-ls@1.0.0 {CWD}/ls-ls-cycle-deps
\`-- a@1.0.0
  \`-- b@1.0.0
    \`-- a@1.0.0 deduped

`

exports[`test/lib/ls.js TAP ls cycle deps with filter args > should print tree output containing deduped ref 1`] = `
[0mtest-npm-ls@1.0.0 {CWD}/ls-ls-cycle-deps-with-filter-args[0m
[0m\`-- [33m[40ma@1.0.0[49m[39m[0m
[0m  \`-- b@1.0.0[0m
[0m    \`-- [33m[40ma@1.0.0[49m[39m [90mdeduped[39m[0m
[0m[0m
`

exports[`test/lib/ls.js TAP ls deduped missing dep > should output parseable signaling missing peer dep in problems 1`] = `
test-npm-ls@1.0.0 {CWD}/ls-ls-deduped-missing-dep
+-- a@1.0.0
| \`-- UNMET DEPENDENCY b@^1.0.0 deduped
\`-- UNMET DEPENDENCY b@^1.0.0

`

exports[`test/lib/ls.js TAP ls default --depth value should be 0 > should output tree containing only top-level dependencies 1`] = `
test-npm-ls@1.0.0 {CWD}/ls-ls-default-depth-value-should-be-0
+-- foo@1.0.0
\`-- lorem@1.0.0

`

exports[`test/lib/ls.js TAP ls empty location > should print empty result 1`] = `
{CWD}/ls-ls-empty-location
\`-- (empty)

`

exports[`test/lib/ls.js TAP ls extraneous deps > should output containing problems info 1`] = `
test-npm-ls@1.0.0 {CWD}/ls-ls-extraneous-deps
+-- foo@1.0.0
| \`-- bar@1.0.0
\`-- lorem@1.0.0 extraneous

`

exports[`test/lib/ls.js TAP ls filter pkg arg using depth option > should list a in top-level only 1`] = `
test-pkg-arg-filter-with-depth-opt@1.0.0 {CWD}/ls-ls-filter-pkg-arg-using-depth-option
\`-- a@1.0.0

`

exports[`test/lib/ls.js TAP ls filter pkg arg using depth option > should print empty results msg 1`] = `
test-pkg-arg-filter-with-depth-opt@1.0.0 {CWD}/ls-ls-filter-pkg-arg-using-depth-option
\`-- (empty)

`

exports[`test/lib/ls.js TAP ls filter pkg arg using depth option > should print expected result 1`] = `
test-pkg-arg-filter-with-depth-opt@1.0.0 {CWD}/ls-ls-filter-pkg-arg-using-depth-option
\`-- b@1.0.0
  \`-- c@1.0.0
    \`-- d@1.0.0

`

exports[`test/lib/ls.js TAP ls filtering by child of missing dep > should print tree and not duplicate child of missing items 1`] = `
filter-by-child-of-missing-dep@1.0.0 {CWD}/ls-ls-filtering-by-child-of-missing-dep
+-- b@1.0.0 extraneous
| \`-- c@1.0.0 deduped
+-- c@1.0.0 extraneous
\`-- d@1.0.0 extraneous
  \`-- c@2.0.0 extraneous

`

exports[`test/lib/ls.js TAP ls from and resolved properties > should not be printed in tree output 1`] = `
test-npm-ls@1.0.0 {CWD}/ls-ls-from-and-resolved-properties
\`-- simple-output@2.1.1

`

exports[`test/lib/ls.js TAP ls global > should print tree and not mark top-level items extraneous 1`] = `
{CWD}/ls-ls-global
+-- a@1.0.0
\`-- b@1.0.0
  \`-- c@1.0.0

`

exports[`test/lib/ls.js TAP ls invalid deduped dep > should output tree signaling mismatching peer dep in problems 1`] = `
[0minvalid-deduped-dep@1.0.0 {CWD}/ls-ls-invalid-deduped-dep[0m
[0m+-- a@1.0.0[0m
[0m| \`-- b@1.0.0 [90mdeduped[39m [31m[40minvalid[49m[39m[0m
[0m\`-- b@1.0.0 [31m[40minvalid[49m[39m[0m
[0m[0m
`

exports[`test/lib/ls.js TAP ls invalid peer dep > should output tree signaling mismatching peer dep in problems 1`] = `
test-npm-ls@1.0.0 {CWD}/ls-ls-invalid-peer-dep
+-- dev-dep@1.0.0
| \`-- foo@1.0.0
|   \`-- bar@1.0.0
+-- lorem@1.0.0
+-- optional-dep@1.0.0
+-- peer-dep@1.0.0 invalid
\`-- prod-dep@1.0.0
  \`-- bar@2.0.0

`

exports[`test/lib/ls.js TAP ls json read problems > should print empty result 1`] = `
{CWD}/ls-ls-json-read-problems
\`-- (empty)

`

exports[`test/lib/ls.js TAP ls loading a tree containing workspaces > should filter single workspace 1`] = `
filter-by-child-of-missing-dep@1.0.0 {CWD}/ls-ls-loading-a-tree-containing-workspaces
\`-- a@1.0.0 -> {CWD}/ls-ls-loading-a-tree-containing-workspaces/a

`

exports[`test/lib/ls.js TAP ls loading a tree containing workspaces > should list workspaces properly 1`] = `
filter-by-child-of-missing-dep@1.0.0 {CWD}/ls-ls-loading-a-tree-containing-workspaces
+-- a@1.0.0 -> {CWD}/ls-ls-loading-a-tree-containing-workspaces/a
| \`-- c@1.0.0
\`-- b@1.0.0 -> {CWD}/ls-ls-loading-a-tree-containing-workspaces/b

`

exports[`test/lib/ls.js TAP ls missing package.json > should log all extraneous deps on error msg 1`] = `
extraneous: bar@1.0.0 {CWD}/ls-ls-missing-package-json/node_modules/bar
extraneous: foo@1.0.0 {CWD}/ls-ls-missing-package-json/node_modules/foo
extraneous: lorem@1.0.0 {CWD}/ls-ls-missing-package-json/node_modules/lorem
`

exports[`test/lib/ls.js TAP ls missing package.json > should output tree missing name/version of top-level package 1`] = `
{CWD}/ls-ls-missing-package-json
+-- bar@1.0.0 extraneous
+-- foo@1.0.0 extraneous
| \`-- bar@1.0.0 deduped
\`-- lorem@1.0.0 extraneous

`

exports[`test/lib/ls.js TAP ls missing/invalid/extraneous > should output tree containing missing, invalid, extraneous labels 1`] = `
test-npm-ls@1.0.0 {CWD}/ls-ls-missing-invalid-extraneous
+-- foo@1.0.0 invalid
| \`-- bar@1.0.0
+-- UNMET DEPENDENCY ipsum@^1.0.0
\`-- lorem@1.0.0 extraneous

`

exports[`test/lib/ls.js TAP ls no args > should output tree representation of dependencies structure 1`] = `
test-npm-ls@1.0.0 {CWD}/ls-ls-no-args
+-- foo@1.0.0
| \`-- bar@1.0.0
\`-- lorem@1.0.0

`

exports[`test/lib/ls.js TAP ls print deduped symlinks > should output tree containing linked deps 1`] = `
print-deduped-symlinks@1.0.0 {CWD}/ls-ls-print-deduped-symlinks
+-- a@1.0.0
| \`-- b@1.0.0 deduped -> {CWD}/ls-ls-print-deduped-symlinks/b
\`-- b@1.0.0 -> {CWD}/ls-ls-print-deduped-symlinks/b

`

exports[`test/lib/ls.js TAP ls resolved points to git ref > should output tree containing git refs 1`] = `
test-npm-ls@1.0.0 {CWD}/ls-ls-resolved-points-to-git-ref
\`-- abbrev@1.1.1 (git+ssh://git@github.com/isaacs/abbrev-js.git#b8f3a2fc0c3bb8ffd8b0d0072cc6b5a3667e963c)

`

exports[`test/lib/ls.js TAP ls unmet optional dep > should output tree with empty entry for missing optional deps 1`] = `
[0mtest-npm-ls@1.0.0 {CWD}/ls-ls-unmet-optional-dep[0m
[0m+-- dev-dep@1.0.0[0m
[0m| \`-- foo@1.0.0[0m
[0m|   \`-- bar@1.0.0[0m
[0m+-- lorem@1.0.0[0m
[0m+-- [33m[40mUNMET OPTIONAL DEPENDENCY[49m[39m missing-optional-dep@^1.0.0[0m
[0m+-- optional-dep@1.0.0 [31m[40minvalid[49m[39m[0m
[0m+-- peer-dep@1.0.0[0m
[0m\`-- prod-dep@1.0.0[0m
[0m  \`-- bar@2.0.0[0m
[0m[0m
`

exports[`test/lib/ls.js TAP ls unmet peer dep > should output tree signaling missing peer dep in problems 1`] = `
test-npm-ls@1.0.0 {CWD}/ls-ls-unmet-peer-dep
\`-- UNMET DEPENDENCY peer-dep@*

`

exports[`test/lib/ls.js TAP ls using aliases > should output tree containing aliases 1`] = `
test-npm-ls@1.0.0 {CWD}/ls-ls-using-aliases
\`-- a@npm:b@1.0.0

`

exports[`test/lib/ls.js TAP ls with args and dedupe entries > should print tree output containing deduped ref 1`] = `
[0mdedupe-entries@1.0.0 {CWD}/ls-ls-with-args-and-dedupe-entries[0m
[0m+-- @npmcli/a@1.0.0[0m
[0m| \`-- [33m[40m@npmcli/b@1.1.2[49m[39m [90mdeduped[39m[0m
[0m+-- [33m[40m@npmcli/b@1.1.2[49m[39m[0m
[0m\`-- @npmcli/c@1.0.0[0m
[0m  \`-- [33m[40m@npmcli/b@1.1.2[49m[39m [90mdeduped[39m[0m
[0m[0m
`

exports[`test/lib/ls.js TAP ls with args and different order of items > should print tree output containing deduped ref 1`] = `
dedupe-entries@1.0.0 {CWD}/ls-ls-with-args-and-different-order-of-items
+-- @npmcli/a@1.0.0
| \`-- @npmcli/c@1.0.0 deduped
+-- @npmcli/b@1.1.2
| \`-- @npmcli/c@1.0.0 deduped
\`-- @npmcli/c@1.0.0

`

exports[`test/lib/ls.js TAP ls with dot filter arg > should output tree contaning only occurrences of filtered by package and colored output 1`] = `
test-npm-ls@1.0.0 {CWD}/ls-ls-with-dot-filter-arg
\`-- (empty)

`

exports[`test/lib/ls.js TAP ls with filter arg > should output tree contaning only occurrences of filtered by package and colored output 1`] = `
[0mtest-npm-ls@1.0.0 {CWD}/ls-ls-with-filter-arg[0m
[0m\`-- [33m[40mlorem@1.0.0[49m[39m[0m
[0m[0m
`

exports[`test/lib/ls.js TAP ls with filter arg nested dep > should output tree contaning only occurrences of filtered package and its ancestors 1`] = `
test-npm-ls@1.0.0 {CWD}/ls-ls-with-filter-arg-nested-dep
\`-- foo@1.0.0
  \`-- bar@1.0.0

`

exports[`test/lib/ls.js TAP ls with missing filter arg > should output tree containing no dependencies info 1`] = `
test-npm-ls@1.0.0 {CWD}/ls-ls-with-missing-filter-arg
\`-- (empty)

`

exports[`test/lib/ls.js TAP ls with multiple filter args > should output tree contaning only occurrences of multiple filtered packages and their ancestors 1`] = `
test-npm-ls@1.0.0 {CWD}/ls-ls-with-multiple-filter-args
+-- foo@1.0.0
| \`-- bar@1.0.0
\`-- lorem@1.0.0

`

exports[`test/lib/ls.js TAP ls with no args dedupe entries > should print tree output containing deduped ref 1`] = `
dedupe-entries@1.0.0 {CWD}/ls-ls-with-no-args-dedupe-entries
+-- @npmcli/a@1.0.0
| \`-- @npmcli/b@1.1.2 deduped
+-- @npmcli/b@1.1.2
\`-- @npmcli/c@1.0.0
  \`-- @npmcli/b@1.1.2 deduped

`

exports[`test/lib/ls.js TAP ls with no args dedupe entries and not displaying all > should print tree output containing deduped ref 1`] = `
dedupe-entries@1.0.0 {CWD}/ls-ls-with-no-args-dedupe-entries-and-not-displaying-all
+-- @npmcli/a@1.0.0
+-- @npmcli/b@1.1.2
\`-- @npmcli/c@1.0.0

`
