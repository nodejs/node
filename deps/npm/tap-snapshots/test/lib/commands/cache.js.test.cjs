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
