/* IMPORTANT
 * This snapshot file is auto-generated, but designed for humans.
 * It should be checked into source control and tracked carefully.
 * Re-generate by setting TAP_SNAPSHOT=1 and running tests.
 * Make sure to inspect the output below.  Do not ignore changes!
 */
'use strict'
exports[`test/lib/commands/cache.js TAP cache ls > logs cache entries 1`] = `
make-fetch-happen:request-cache:https://registry.npmjs.org/test-package
make-fetch-happen:request-cache:https://registry.npmjs.org/test-package/-/test-package-1.0.0.tgz
`

exports[`test/lib/commands/cache.js TAP cache ls corrupted > logs cache entries with bad data 1`] = `
make-fetch-happen:request-cache:https://registry.npmjs.org/corrupted
make-fetch-happen:request-cache:https://registry.npmjs.org/corrupted/-/corrupted-3.1.0.tgz
`

exports[`test/lib/commands/cache.js TAP cache ls missing packument version not an object > logs cache entry for packument 1`] = `
make-fetch-happen:request-cache:https://registry.npmjs.org/missing-version
`

exports[`test/lib/commands/cache.js TAP cache ls nonpublic registry > logs cache entry for extemporaneously and its tarball 1`] = `
make-fetch-happen:request-cache:https://somerepo.github.org/aabbcc/
make-fetch-happen:request-cache:https://somerepo.github.org/extemporaneously
`

exports[`test/lib/commands/cache.js TAP cache ls pkgs > logs cache entries for npm and webpack and one webpack tgz 1`] = `
make-fetch-happen:request-cache:https://registry.npmjs.org/npm
make-fetch-happen:request-cache:https://registry.npmjs.org/npm/-/npm-1.2.0.tgz
make-fetch-happen:request-cache:https://registry.npmjs.org/webpack
make-fetch-happen:request-cache:https://registry.npmjs.org/webpack/-/webpack-4.47.0.tgz
`

exports[`test/lib/commands/cache.js TAP cache ls scoped and scoped slash > logs cache entries for @gar and @fritzy 1`] = `
make-fetch-happen:request-cache:https://registry.npmjs.org/@fritzy%2fstaydown
make-fetch-happen:request-cache:https://registry.npmjs.org/@gar%2fnpm-expansion
`

exports[`test/lib/commands/cache.js TAP cache ls special > logs cache entries for foo 1`] = `
make-fetch-happen:request-cache:https://registry.npmjs.org/foo
make-fetch-happen:request-cache:https://registry.npmjs.org/foo/-/foo-1.2.3-beta.tgz
`

exports[`test/lib/commands/cache.js TAP cache npx info: valid and invalid entry > shows invalid package info 1`] = `
invalid npx cache entry with key deadbeef
location: {CWD}/cache/_npx/deadbeef

invalid npx cache entry with key badc0de
location: {CWD}/cache/_npx/badc0de
`

exports[`test/lib/commands/cache.js TAP cache npx info: valid and invalid entry > shows valid package info 1`] = `
invalid npx cache entry with key deadbeef
location: {CWD}/cache/_npx/deadbeef
`

exports[`test/lib/commands/cache.js TAP cache npx info: valid entry with _npx directory package > shows valid package info with _npx directory package 1`] = `
valid npx cache entry with key valid123
location: {CWD}/cache/_npx/valid123
packages:
- /path/to/valid-package
`

exports[`test/lib/commands/cache.js TAP cache npx info: valid entry with _npx packages > shows valid package info with _npx packages 1`] = `
valid npx cache entry with key valid123
location: {CWD}/cache/_npx/valid123
packages:
- valid-package@1.0.0 (valid-package@1.0.0)
`

exports[`test/lib/commands/cache.js TAP cache npx info: valid entry with a link dependency > shows link dependency realpath (child.isLink branch) 1`] = `
valid npx cache entry with key link123
location: {CWD}/cache/_npx/link123
packages: (unknown)
dependencies:
- {CWD}/cache/_npx/some-other-loc
`

exports[`test/lib/commands/cache.js TAP cache npx info: valid entry with dependencies > shows valid package info with dependencies 1`] = `
valid npx cache entry with key valid456
location: {CWD}/cache/_npx/valid456
packages: (unknown)
dependencies:
- dep-package@1.0.0
`

exports[`test/lib/commands/cache.js TAP cache npx ls: empty cache > logs message for empty npx cache 1`] = `
npx cache does not exist
`

exports[`test/lib/commands/cache.js TAP cache npx ls: entry with unknown package > lists entry with unknown package 1`] = `
unknown123: (unknown)
`

exports[`test/lib/commands/cache.js TAP cache npx ls: some entries > lists one valid and one invalid entry 1`] = `
abc123: fake-npx-package@1.0.0
z9y8x7: (empty/invalid)
`

exports[`test/lib/commands/cache.js TAP cache npx rm: remove single entry > logs removing single npx cache entry 1`] = `
Removing npx key at {CWD}/cache/_npx/123removeme
Removing npx key at {CWD}/cache/_npx/123removeme
`

exports[`test/lib/commands/cache.js TAP cache npx rm: removing all with --force works > logs removing everything 1`] = `
Removing npx key at {CWD}/cache/_npx/remove-all-yes-force
`

exports[`test/lib/commands/cache.js TAP cache npx rm: removing all without --force fails > logs usage error when removing all without --force 1`] = `

`

exports[`test/lib/commands/cache.js TAP cache npx rm: removing more than 1, less than all entries > logs removing 2 of 3 entries 1`] = `
Removing npx key at {CWD}/cache/_npx/123removeme
Removing npx key at {CWD}/cache/_npx/456removeme
Removing npx key at {CWD}/cache/_npx/123removeme
Removing npx key at {CWD}/cache/_npx/456removeme
`

exports[`test/lib/commands/cache.js TAP cache rm > logs deleting single entry 1`] = `
Deleted: make-fetch-happen:request-cache:https://registry.npmjs.org/test-package/-/test-package-1.0.0.tgz
`

exports[`test/lib/commands/cache.js TAP cache verify > shows verified cache output 1`] = `
Cache verified and compressed ({PATH})
Content verified: 0 (0 bytes)
Index entries: 0
Finished in xxxs
`

exports[`test/lib/commands/cache.js TAP cache verify w/ extra output > shows extra output 1`] = `
Cache verified and compressed ({PATH})
Content verified: 17057 (1644485260 bytes)
Corrupted content removed: 12345
Content garbage-collected: 1144 (248164665 bytes)
Missing content: 92
Index entries: 20175
Finished in xxxs
`
