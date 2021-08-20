/* IMPORTANT
 * This snapshot file is auto-generated, but designed for humans.
 * It should be checked into source control and tracked carefully.
 * Re-generate by setting TAP_SNAPSHOT=1 and running tests.
 * Make sure to inspect the output below.  Do not ignore changes!
 */
'use strict'
exports[`test/lib/link.js TAP link global linked pkg to local nm when using args > should create a local symlink to global pkg 1`] = `
{CWD}/test/lib/tap-testdir-link-link-global-linked-pkg-to-local-nm-when-using-args/my-project/node_modules/@myscope/bar -> {CWD}/test/lib/tap-testdir-link-link-global-linked-pkg-to-local-nm-when-using-args/global-prefix/lib/node_modules/@myscope/bar
{CWD}/test/lib/tap-testdir-link-link-global-linked-pkg-to-local-nm-when-using-args/my-project/node_modules/@myscope/linked -> {CWD}/test/lib/tap-testdir-link-link-global-linked-pkg-to-local-nm-when-using-args/scoped-linked
{CWD}/test/lib/tap-testdir-link-link-global-linked-pkg-to-local-nm-when-using-args/my-project/node_modules/a -> {CWD}/test/lib/tap-testdir-link-link-global-linked-pkg-to-local-nm-when-using-args/global-prefix/lib/node_modules/a
{CWD}/test/lib/tap-testdir-link-link-global-linked-pkg-to-local-nm-when-using-args/my-project/node_modules/link-me-too -> {CWD}/test/lib/tap-testdir-link-link-global-linked-pkg-to-local-nm-when-using-args/link-me-too
{CWD}/test/lib/tap-testdir-link-link-global-linked-pkg-to-local-nm-when-using-args/my-project/node_modules/test-pkg-link -> {CWD}/test/lib/tap-testdir-link-link-global-linked-pkg-to-local-nm-when-using-args/test-pkg-link

`

exports[`test/lib/link.js TAP link global linked pkg to local workspace using args > should create a local symlink to global pkg 1`] = `
{CWD}/test/lib/tap-testdir-link-link-global-linked-pkg-to-local-workspace-using-args/my-project/node_modules/@myscope/bar -> {CWD}/test/lib/tap-testdir-link-link-global-linked-pkg-to-local-workspace-using-args/global-prefix/lib/node_modules/@myscope/bar
{CWD}/test/lib/tap-testdir-link-link-global-linked-pkg-to-local-workspace-using-args/my-project/node_modules/@myscope/linked -> {CWD}/test/lib/tap-testdir-link-link-global-linked-pkg-to-local-workspace-using-args/scoped-linked
{CWD}/test/lib/tap-testdir-link-link-global-linked-pkg-to-local-workspace-using-args/my-project/node_modules/a -> {CWD}/test/lib/tap-testdir-link-link-global-linked-pkg-to-local-workspace-using-args/global-prefix/lib/node_modules/a
{CWD}/test/lib/tap-testdir-link-link-global-linked-pkg-to-local-workspace-using-args/my-project/node_modules/link-me-too -> {CWD}/test/lib/tap-testdir-link-link-global-linked-pkg-to-local-workspace-using-args/link-me-too
{CWD}/test/lib/tap-testdir-link-link-global-linked-pkg-to-local-workspace-using-args/my-project/node_modules/test-pkg-link -> {CWD}/test/lib/tap-testdir-link-link-global-linked-pkg-to-local-workspace-using-args/test-pkg-link
{CWD}/test/lib/tap-testdir-link-link-global-linked-pkg-to-local-workspace-using-args/my-project/node_modules/x -> {CWD}/test/lib/tap-testdir-link-link-global-linked-pkg-to-local-workspace-using-args/my-project/packages/x

`

exports[`test/lib/link.js TAP link pkg already in global space > should create a local symlink to global pkg 1`] = `
{CWD}/test/lib/tap-testdir-link-link-pkg-already-in-global-space/my-project/node_modules/@myscope/linked -> {CWD}/test/lib/tap-testdir-link-link-pkg-already-in-global-space/scoped-linked

`

exports[`test/lib/link.js TAP link pkg already in global space when prefix is a symlink > should create a local symlink to global pkg 1`] = `
{CWD}/test/lib/tap-testdir-link-link-pkg-already-in-global-space-when-prefix-is-a-symlink/my-project/node_modules/@myscope/linked -> {CWD}/test/lib/tap-testdir-link-link-pkg-already-in-global-space-when-prefix-is-a-symlink/scoped-linked

`

exports[`test/lib/link.js TAP link to globalDir when in current working dir of pkg and no args > should create a global link to current pkg 1`] = `
{CWD}/test/lib/tap-testdir-link-link-to-globalDir-when-in-current-working-dir-of-pkg-and-no-args/global-prefix/lib/node_modules/test-pkg-link -> {CWD}/test/lib/tap-testdir-link-link-to-globalDir-when-in-current-working-dir-of-pkg-and-no-args/test-pkg-link

`

exports[`test/lib/link.js TAP link ws to globalDir when workspace specified and no args > should create a global link to current pkg 1`] = `
{CWD}/test/lib/tap-testdir-link-link-ws-to-globalDir-when-workspace-specified-and-no-args/global-prefix/lib/node_modules/a -> {CWD}/test/lib/tap-testdir-link-link-ws-to-globalDir-when-workspace-specified-and-no-args/test-pkg-link/packages/a

`
