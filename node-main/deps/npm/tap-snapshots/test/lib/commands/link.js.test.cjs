/* IMPORTANT
 * This snapshot file is auto-generated, but designed for humans.
 * It should be checked into source control and tracked carefully.
 * Re-generate by setting TAP_SNAPSHOT=1 and running tests.
 * Make sure to inspect the output below.  Do not ignore changes!
 */
'use strict'
exports[`test/lib/commands/link.js TAP hash character in working directory path > should create a global link to current pkg, even within path with hash 1`] = `
{CWD}/global/node_modules/test-pkg-link -> {CWD}/other/i_like_#_in_my_paths/test-pkg-link

`

exports[`test/lib/commands/link.js TAP link global linked pkg to local nm when using args > should create a local symlink to global pkg 1`] = `
{CWD}/prefix/node_modules/@myscope/bar -> {CWD}/global/node_modules/@myscope/bar
{CWD}/prefix/node_modules/@myscope/linked -> {CWD}/other/scoped-linked
{CWD}/prefix/node_modules/a -> {CWD}/global/node_modules/a
{CWD}/prefix/node_modules/link-me-too -> {CWD}/other/link-me-too
{CWD}/prefix/node_modules/test-pkg-link -> {CWD}/other/test-pkg-link

`

exports[`test/lib/commands/link.js TAP link global linked pkg to local workspace using args > should create a local symlink to global pkg 1`] = `
{CWD}/prefix/node_modules/@myscope/bar -> {CWD}/global/node_modules/@myscope/bar
{CWD}/prefix/node_modules/@myscope/linked -> {CWD}/other/scoped-linked
{CWD}/prefix/node_modules/a -> {CWD}/global/node_modules/a
{CWD}/prefix/node_modules/link-me-too -> {CWD}/other/link-me-too
{CWD}/prefix/node_modules/test-pkg-link -> {CWD}/other/test-pkg-link
{CWD}/prefix/node_modules/x -> {CWD}/prefix/packages/x

`

exports[`test/lib/commands/link.js TAP link pkg already in global space > should create a local symlink to global pkg 1`] = `
{CWD}/prefix/node_modules/@myscope/linked -> {CWD}/other/scoped-linked

`

exports[`test/lib/commands/link.js TAP link pkg already in global space when prefix is a symlink > should create a local symlink to global pkg 1`] = `
{CWD}/prefix/node_modules/@myscope/linked -> {CWD}/other/scoped-linked

`

exports[`test/lib/commands/link.js TAP link to globalDir when in current working dir of pkg and no args > should create a global link to current pkg 1`] = `
{CWD}/global/node_modules/test-pkg-link -> {CWD}/prefix

`

exports[`test/lib/commands/link.js TAP link ws to globalDir when workspace specified and no args > should create a global link to current pkg 1`] = `
{CWD}/global/node_modules/a -> {CWD}/prefix/packages/a

`

exports[`test/lib/commands/link.js TAP test linked installed as symlinks > linked package should not be installed 1`] = `
{CWD}/prefix/node_modules/mylink -> {CWD}/other/mylink

`
