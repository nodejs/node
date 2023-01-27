/* IMPORTANT
 * This snapshot file is auto-generated, but designed for humans.
 * It should be checked into source control and tracked carefully.
 * Re-generate by setting TAP_SNAPSHOT=1 and running tests.
 * Make sure to inspect the output below.  Do not ignore changes!
 */
'use strict'
exports[`test/lib/commands/doctor.js TAP all clear > logs 1`] = `
Object {
  "error": Array [],
  "info": Array [
    Array [
      "Running checkup",
    ],
    Array [
      "checkPing",
      "Pinging registry",
    ],
    Array [
      "getLatestNpmVersion",
      "Getting npm package information",
    ],
    Array [
      "getLatestNodejsVersion",
      "Getting Node.js release information",
    ],
    Array [
      "getGitPath",
      "Finding git in your PATH",
    ],
    Array [
      "getBinPath",
      "Finding npm global bin in your PATH",
    ],
    Array [
      "verifyCachedFiles",
      "Verifying the npm cache",
    ],
    Array [
      "verifyCachedFiles",
      String(
        Verification complete. Stats: {
          "badContentCount": 0,
          "reclaimedCount": 0,
          "missingContent": 0,
          "verifiedContent": 0
        }
      ),
    ],
  ],
  "warn": Array [],
}
`

exports[`test/lib/commands/doctor.js TAP all clear > output 1`] = `
Check                             [90m  [39mValue [90m  [39mRecommendation/Notes
npm ping                          [90m  [39mok    [90m  [39m
npm -v                            [90m  [39mok    [90m  [39mcurrent: v1.0.0, latest: v1.0.0
node -v                           [90m  [39mok    [90m  [39mcurrent: v1.0.0, recommended: v1.0.0
npm config get registry           [90m  [39mok    [90m  [39musing default registry (https://registry.npmjs.org/)
git executable in PATH            [90m  [39mok    [90m  [39m/path/to/git
global bin folder in PATH         [90m  [39mok    [90m  [39m{CWD}/global/bin
Perms check on cached files       [90m  [39mok    [90m  [39m
Perms check on local node_modules [90m  [39mok    [90m  [39m
Perms check on global node_modules[90m  [39mok    [90m  [39m
Perms check on local bin folder   [90m  [39mok    [90m  [39m
Perms check on global bin folder  [90m  [39mok    [90m  [39m
Verify cache contents             [90m  [39mok    [90m  [39mverified 0 tarballs
`

exports[`test/lib/commands/doctor.js TAP all clear in color > everything is ok in color 1`] = `
[4mCheck[24m                             [90m  [39m[4mValue[24m [90m  [39m[4mRecommendation/Notes[24m
npm ping                          [90m  [39m[32mok[39m    [90m  [39m
npm -v                            [90m  [39m[32mok[39m    [90m  [39mcurrent: v1.0.0, latest: v1.0.0
node -v                           [90m  [39m[32mok[39m    [90m  [39mcurrent: v1.0.0, recommended: v1.0.0
npm config get registry           [90m  [39m[32mok[39m    [90m  [39musing default registry (https://registry.npmjs.org/)
git executable in PATH            [90m  [39m[32mok[39m    [90m  [39m/path/to/git
global bin folder in PATH         [90m  [39m[32mok[39m    [90m  [39m{CWD}/global/bin
Perms check on cached files       [90m  [39m[32mok[39m    [90m  [39m
Perms check on local node_modules [90m  [39m[32mok[39m    [90m  [39m
Perms check on global node_modules[90m  [39m[32mok[39m    [90m  [39m
Perms check on local bin folder   [90m  [39m[32mok[39m    [90m  [39m
Perms check on global bin folder  [90m  [39m[32mok[39m    [90m  [39m
Verify cache contents             [90m  [39m[32mok[39m    [90m  [39mverified 0 tarballs
`

exports[`test/lib/commands/doctor.js TAP all clear in color > logs 1`] = `
Object {
  "error": Array [],
  "info": Array [
    Array [
      "Running checkup",
    ],
    Array [
      "checkPing",
      "Pinging registry",
    ],
    Array [
      "getLatestNpmVersion",
      "Getting npm package information",
    ],
    Array [
      "getLatestNodejsVersion",
      "Getting Node.js release information",
    ],
    Array [
      "getGitPath",
      "Finding git in your PATH",
    ],
    Array [
      "getBinPath",
      "Finding npm global bin in your PATH",
    ],
    Array [
      "verifyCachedFiles",
      "Verifying the npm cache",
    ],
    Array [
      "verifyCachedFiles",
      String(
        Verification complete. Stats: {
          "badContentCount": 0,
          "reclaimedCount": 0,
          "missingContent": 0,
          "verifiedContent": 0
        }
      ),
    ],
  ],
  "warn": Array [],
}
`

exports[`test/lib/commands/doctor.js TAP bad proxy > logs 1`] = `
Object {
  "error": Array [],
  "info": Array [
    Array [
      "Running checkup",
    ],
    Array [
      "checkPing",
      "Pinging registry",
    ],
    Array [
      "getLatestNpmVersion",
      "Getting npm package information",
    ],
    Array [
      "getLatestNodejsVersion",
      "Getting Node.js release information",
    ],
    Array [
      "getGitPath",
      "Finding git in your PATH",
    ],
    Array [
      "getBinPath",
      "Finding npm global bin in your PATH",
    ],
    Array [
      "verifyCachedFiles",
      "Verifying the npm cache",
    ],
    Array [
      "verifyCachedFiles",
      String(
        Verification complete. Stats: {
          "badContentCount": 0,
          "reclaimedCount": 0,
          "missingContent": 0,
          "verifiedContent": 0
        }
      ),
    ],
  ],
  "warn": Array [],
}
`

exports[`test/lib/commands/doctor.js TAP bad proxy > output 1`] = `
Check                             [90m  [39mValue [90m  [39mRecommendation/Notes
npm ping                          [90m  [39mnot ok[90m  [39munsupported proxy protocol: 'ssh:'
npm -v                            [90m  [39mnot ok[90m  [39mError: unsupported proxy protocol: 'ssh:'
node -v                           [90m  [39mnot ok[90m  [39mError: unsupported proxy protocol: 'ssh:'
npm config get registry           [90m  [39mok    [90m  [39musing default registry (https://registry.npmjs.org/)
git executable in PATH            [90m  [39mok    [90m  [39m/path/to/git
global bin folder in PATH         [90m  [39mok    [90m  [39m{CWD}/global/bin
Perms check on cached files       [90m  [39mok    [90m  [39m
Perms check on local node_modules [90m  [39mok    [90m  [39m
Perms check on global node_modules[90m  [39mok    [90m  [39m
Perms check on local bin folder   [90m  [39mok    [90m  [39m
Perms check on global bin folder  [90m  [39mok    [90m  [39m
Verify cache contents             [90m  [39mok    [90m  [39mverified 0 tarballs
`

exports[`test/lib/commands/doctor.js TAP cacache badContent > corrupted cache content 1`] = `
Check                             [90m  [39mValue [90m  [39mRecommendation/Notes
npm ping                          [90m  [39mok    [90m  [39m
npm -v                            [90m  [39mok    [90m  [39mcurrent: v1.0.0, latest: v1.0.0
node -v                           [90m  [39mok    [90m  [39mcurrent: v1.0.0, recommended: v1.0.0
npm config get registry           [90m  [39mok    [90m  [39musing default registry (https://registry.npmjs.org/)
git executable in PATH            [90m  [39mok    [90m  [39m/path/to/git
global bin folder in PATH         [90m  [39mok    [90m  [39m{CWD}/global/bin
Perms check on cached files       [90m  [39mok    [90m  [39m
Perms check on local node_modules [90m  [39mok    [90m  [39m
Perms check on global node_modules[90m  [39mok    [90m  [39m
Perms check on local bin folder   [90m  [39mok    [90m  [39m
Perms check on global bin folder  [90m  [39mok    [90m  [39m
Verify cache contents             [90m  [39mok    [90m  [39mverified 2 tarballs
`

exports[`test/lib/commands/doctor.js TAP cacache badContent > logs 1`] = `
Object {
  "error": Array [],
  "info": Array [
    Array [
      "Running checkup",
    ],
    Array [
      "checkPing",
      "Pinging registry",
    ],
    Array [
      "getLatestNpmVersion",
      "Getting npm package information",
    ],
    Array [
      "getLatestNodejsVersion",
      "Getting Node.js release information",
    ],
    Array [
      "getGitPath",
      "Finding git in your PATH",
    ],
    Array [
      "getBinPath",
      "Finding npm global bin in your PATH",
    ],
    Array [
      "verifyCachedFiles",
      "Verifying the npm cache",
    ],
    Array [
      "verifyCachedFiles",
      String(
        Verification complete. Stats: {
          "badContentCount": 1,
          "reclaimedCount": 0,
          "missingContent": 0,
          "verifiedContent": 2
        }
      ),
    ],
  ],
  "warn": Array [
    Array [
      "verifyCachedFiles",
      "Corrupted content removed: 1",
    ],
    Array [
      "verifyCachedFiles",
      "Cache issues have been fixed",
    ],
  ],
}
`

exports[`test/lib/commands/doctor.js TAP cacache missingContent > logs 1`] = `
Object {
  "error": Array [],
  "info": Array [
    Array [
      "Running checkup",
    ],
    Array [
      "checkPing",
      "Pinging registry",
    ],
    Array [
      "getLatestNpmVersion",
      "Getting npm package information",
    ],
    Array [
      "getLatestNodejsVersion",
      "Getting Node.js release information",
    ],
    Array [
      "getGitPath",
      "Finding git in your PATH",
    ],
    Array [
      "getBinPath",
      "Finding npm global bin in your PATH",
    ],
    Array [
      "verifyCachedFiles",
      "Verifying the npm cache",
    ],
    Array [
      "verifyCachedFiles",
      String(
        Verification complete. Stats: {
          "badContentCount": 0,
          "reclaimedCount": 0,
          "missingContent": 1,
          "verifiedContent": 2
        }
      ),
    ],
  ],
  "warn": Array [
    Array [
      "verifyCachedFiles",
      "Missing content: 1",
    ],
    Array [
      "verifyCachedFiles",
      "Cache issues have been fixed",
    ],
  ],
}
`

exports[`test/lib/commands/doctor.js TAP cacache missingContent > missing content 1`] = `
Check                             [90m  [39mValue [90m  [39mRecommendation/Notes
npm ping                          [90m  [39mok    [90m  [39m
npm -v                            [90m  [39mok    [90m  [39mcurrent: v1.0.0, latest: v1.0.0
node -v                           [90m  [39mok    [90m  [39mcurrent: v1.0.0, recommended: v1.0.0
npm config get registry           [90m  [39mok    [90m  [39musing default registry (https://registry.npmjs.org/)
git executable in PATH            [90m  [39mok    [90m  [39m/path/to/git
global bin folder in PATH         [90m  [39mok    [90m  [39m{CWD}/global/bin
Perms check on cached files       [90m  [39mok    [90m  [39m
Perms check on local node_modules [90m  [39mok    [90m  [39m
Perms check on global node_modules[90m  [39mok    [90m  [39m
Perms check on local bin folder   [90m  [39mok    [90m  [39m
Perms check on global bin folder  [90m  [39mok    [90m  [39m
Verify cache contents             [90m  [39mok    [90m  [39mverified 2 tarballs
`

exports[`test/lib/commands/doctor.js TAP cacache reclaimedCount > content garbage collected 1`] = `
Check                             [90m  [39mValue [90m  [39mRecommendation/Notes
npm ping                          [90m  [39mok    [90m  [39m
npm -v                            [90m  [39mok    [90m  [39mcurrent: v1.0.0, latest: v1.0.0
node -v                           [90m  [39mok    [90m  [39mcurrent: v1.0.0, recommended: v1.0.0
npm config get registry           [90m  [39mok    [90m  [39musing default registry (https://registry.npmjs.org/)
git executable in PATH            [90m  [39mok    [90m  [39m/path/to/git
global bin folder in PATH         [90m  [39mok    [90m  [39m{CWD}/global/bin
Perms check on cached files       [90m  [39mok    [90m  [39m
Perms check on local node_modules [90m  [39mok    [90m  [39m
Perms check on global node_modules[90m  [39mok    [90m  [39m
Perms check on local bin folder   [90m  [39mok    [90m  [39m
Perms check on global bin folder  [90m  [39mok    [90m  [39m
Verify cache contents             [90m  [39mok    [90m  [39mverified 2 tarballs
`

exports[`test/lib/commands/doctor.js TAP cacache reclaimedCount > logs 1`] = `
Object {
  "error": Array [],
  "info": Array [
    Array [
      "Running checkup",
    ],
    Array [
      "checkPing",
      "Pinging registry",
    ],
    Array [
      "getLatestNpmVersion",
      "Getting npm package information",
    ],
    Array [
      "getLatestNodejsVersion",
      "Getting Node.js release information",
    ],
    Array [
      "getGitPath",
      "Finding git in your PATH",
    ],
    Array [
      "getBinPath",
      "Finding npm global bin in your PATH",
    ],
    Array [
      "verifyCachedFiles",
      "Verifying the npm cache",
    ],
    Array [
      "verifyCachedFiles",
      String(
        Verification complete. Stats: {
          "badContentCount": 0,
          "reclaimedCount": 1,
          "missingContent": 0,
          "verifiedContent": 2
        }
      ),
    ],
  ],
  "warn": Array [
    Array [
      "verifyCachedFiles",
      "Content garbage-collected: 1 (undefined bytes)",
    ],
    Array [
      "verifyCachedFiles",
      "Cache issues have been fixed",
    ],
  ],
}
`

exports[`test/lib/commands/doctor.js TAP discrete checks cache > logs 1`] = `
Object {
  "error": Array [],
  "info": Array [
    Array [
      "Running checkup",
    ],
    Array [
      "verifyCachedFiles",
      "Verifying the npm cache",
    ],
    Array [
      "verifyCachedFiles",
      String(
        Verification complete. Stats: {
          "badContentCount": 0,
          "reclaimedCount": 0,
          "missingContent": 0,
          "verifiedContent": 0
        }
      ),
    ],
  ],
  "warn": Array [],
}
`

exports[`test/lib/commands/doctor.js TAP discrete checks cache > output 1`] = `
Check                      [90m  [39mValue [90m  [39mRecommendation/Notes
Perms check on cached files[90m  [39mok    [90m  [39m
Verify cache contents      [90m  [39mok    [90m  [39mverified 0 tarballs
`

exports[`test/lib/commands/doctor.js TAP discrete checks git > logs 1`] = `
Object {
  "error": Array [],
  "info": Array [
    Array [
      "Running checkup",
    ],
  ],
  "warn": Array [],
}
`

exports[`test/lib/commands/doctor.js TAP discrete checks git > output 1`] = `
Check[90m  [39mValue [90m  [39mRecommendation/Notes
`

exports[`test/lib/commands/doctor.js TAP discrete checks invalid environment > logs 1`] = `
Object {
  "error": Array [],
  "info": Array [
    Array [
      "Running checkup",
    ],
    Array [
      "getGitPath",
      "Finding git in your PATH",
    ],
    Array [
      "getBinPath",
      "Finding npm global bin in your PATH",
    ],
  ],
  "warn": Array [],
}
`

exports[`test/lib/commands/doctor.js TAP discrete checks invalid environment > output 1`] = `
Check                    [90m  [39mValue [90m  [39mRecommendation/Notes
git executable in PATH   [90m  [39mok    [90m  [39m/path/to/git
global bin folder in PATH[90m  [39mnot ok[90m  [39mError: Add {CWD}/global/bin to your $PATH
`

exports[`test/lib/commands/doctor.js TAP discrete checks permissions - not windows > logs 1`] = `
Object {
  "error": Array [],
  "info": Array [
    Array [
      "Running checkup",
    ],
  ],
  "warn": Array [],
}
`

exports[`test/lib/commands/doctor.js TAP discrete checks permissions - not windows > output 1`] = `
Check                             [90m  [39mValue [90m  [39mRecommendation/Notes
Perms check on cached files       [90m  [39mok    [90m  [39m
Perms check on local node_modules [90m  [39mok    [90m  [39m
Perms check on global node_modules[90m  [39mok    [90m  [39m
Perms check on local bin folder   [90m  [39mok    [90m  [39m
Perms check on global bin folder  [90m  [39mok    [90m  [39m
`

exports[`test/lib/commands/doctor.js TAP discrete checks permissions - windows > logs 1`] = `
Object {
  "error": Array [],
  "info": Array [
    Array [
      "Running checkup",
    ],
  ],
  "warn": Array [],
}
`

exports[`test/lib/commands/doctor.js TAP discrete checks permissions - windows > output 1`] = `
Check[90m  [39mValue [90m  [39mRecommendation/Notes
`

exports[`test/lib/commands/doctor.js TAP discrete checks ping > logs 1`] = `
Object {
  "error": Array [],
  "info": Array [
    Array [
      "Running checkup",
    ],
    Array [
      "checkPing",
      "Pinging registry",
    ],
  ],
  "warn": Array [],
}
`

exports[`test/lib/commands/doctor.js TAP discrete checks ping > output 1`] = `
Check   [90m  [39mValue [90m  [39mRecommendation/Notes
npm ping[90m  [39mok    [90m  [39m
`

exports[`test/lib/commands/doctor.js TAP discrete checks registry > logs 1`] = `
Object {
  "error": Array [],
  "info": Array [
    Array [
      "Running checkup",
    ],
    Array [
      "checkPing",
      "Pinging registry",
    ],
  ],
  "warn": Array [],
}
`

exports[`test/lib/commands/doctor.js TAP discrete checks registry > output 1`] = `
Check                  [90m  [39mValue [90m  [39mRecommendation/Notes
npm ping               [90m  [39mok    [90m  [39m
npm config get registry[90m  [39mok    [90m  [39musing default registry (https://registry.npmjs.org/)
`

exports[`test/lib/commands/doctor.js TAP discrete checks versions > logs 1`] = `
Object {
  "error": Array [],
  "info": Array [
    Array [
      "Running checkup",
    ],
    Array [
      "getLatestNpmVersion",
      "Getting npm package information",
    ],
    Array [
      "getLatestNodejsVersion",
      "Getting Node.js release information",
    ],
  ],
  "warn": Array [],
}
`

exports[`test/lib/commands/doctor.js TAP discrete checks versions > output 1`] = `
Check  [90m  [39mValue [90m  [39mRecommendation/Notes
npm -v [90m  [39mok    [90m  [39mcurrent: v1.0.0, latest: v1.0.0
node -v[90m  [39mok    [90m  [39mcurrent: v1.0.0, recommended: v1.0.0
`

exports[`test/lib/commands/doctor.js TAP error reading directory > logs 1`] = `
Object {
  "error": Array [],
  "info": Array [
    Array [
      "Running checkup",
    ],
    Array [
      "checkPing",
      "Pinging registry",
    ],
    Array [
      "getLatestNpmVersion",
      "Getting npm package information",
    ],
    Array [
      "getLatestNodejsVersion",
      "Getting Node.js release information",
    ],
    Array [
      "getGitPath",
      "Finding git in your PATH",
    ],
    Array [
      "getBinPath",
      "Finding npm global bin in your PATH",
    ],
    Array [
      "verifyCachedFiles",
      "Verifying the npm cache",
    ],
    Array [
      "verifyCachedFiles",
      String(
        Verification complete. Stats: {
          "badContentCount": 0,
          "reclaimedCount": 0,
          "missingContent": 0,
          "verifiedContent": 0
        }
      ),
    ],
  ],
  "warn": Array [
    Array [
      "checkFilesPermission",
      "error reading directory {CWD}/cache",
    ],
    Array [
      "checkFilesPermission",
      "error reading directory {CWD}/prefix/node_modules",
    ],
    Array [
      "checkFilesPermission",
      "error reading directory {CWD}/global/node_modules",
    ],
    Array [
      "checkFilesPermission",
      "error reading directory {CWD}/prefix/node_modules/.bin",
    ],
    Array [
      "checkFilesPermission",
      "error reading directory {CWD}/global/bin",
    ],
  ],
}
`

exports[`test/lib/commands/doctor.js TAP error reading directory > readdir error 1`] = `
Check                             [90m  [39mValue [90m  [39mRecommendation/Notes
npm ping                          [90m  [39mok    [90m  [39m
npm -v                            [90m  [39mok    [90m  [39mcurrent: v1.0.0, latest: v1.0.0
node -v                           [90m  [39mok    [90m  [39mcurrent: v1.0.0, recommended: v1.0.0
npm config get registry           [90m  [39mok    [90m  [39musing default registry (https://registry.npmjs.org/)
git executable in PATH            [90m  [39mok    [90m  [39m/path/to/git
global bin folder in PATH         [90m  [39mok    [90m  [39m{CWD}/global/bin
Perms check on cached files       [90m  [39mnot ok[90m  [39mCheck the permissions of files in {CWD}/cache (should be owned by current user)
Perms check on local node_modules [90m  [39mnot ok[90m  [39mCheck the permissions of files in {CWD}/prefix/node_modules (should be owned by current user)
Perms check on global node_modules[90m  [39mnot ok[90m  [39mCheck the permissions of files in {CWD}/global/node_modules
Perms check on local bin folder   [90m  [39mnot ok[90m  [39mCheck the permissions of files in {CWD}/prefix/node_modules/.bin
Perms check on global bin folder  [90m  [39mnot ok[90m  [39mCheck the permissions of files in {CWD}/global/bin
Verify cache contents             [90m  [39mok    [90m  [39mverified 0 tarballs
`

exports[`test/lib/commands/doctor.js TAP incorrect owner > incorrect owner 1`] = `
Check                             [90m  [39mValue [90m  [39mRecommendation/Notes
npm ping                          [90m  [39mok    [90m  [39m
npm -v                            [90m  [39mok    [90m  [39mcurrent: v1.0.0, latest: v1.0.0
node -v                           [90m  [39mok    [90m  [39mcurrent: v1.0.0, recommended: v1.0.0
npm config get registry           [90m  [39mok    [90m  [39musing default registry (https://registry.npmjs.org/)
git executable in PATH            [90m  [39mok    [90m  [39m/path/to/git
global bin folder in PATH         [90m  [39mok    [90m  [39m{CWD}/global/bin
Perms check on cached files       [90m  [39mnot ok[90m  [39mCheck the permissions of files in {CWD}/cache (should be owned by current user)
Perms check on local node_modules [90m  [39mok    [90m  [39m
Perms check on global node_modules[90m  [39mok    [90m  [39m
Perms check on local bin folder   [90m  [39mok    [90m  [39m
Perms check on global bin folder  [90m  [39mok    [90m  [39m
Verify cache contents             [90m  [39mok    [90m  [39mverified 0 tarballs
`

exports[`test/lib/commands/doctor.js TAP incorrect owner > logs 1`] = `
Object {
  "error": Array [],
  "info": Array [
    Array [
      "Running checkup",
    ],
    Array [
      "checkPing",
      "Pinging registry",
    ],
    Array [
      "getLatestNpmVersion",
      "Getting npm package information",
    ],
    Array [
      "getLatestNodejsVersion",
      "Getting Node.js release information",
    ],
    Array [
      "getGitPath",
      "Finding git in your PATH",
    ],
    Array [
      "getBinPath",
      "Finding npm global bin in your PATH",
    ],
    Array [
      "verifyCachedFiles",
      "Verifying the npm cache",
    ],
    Array [
      "verifyCachedFiles",
      String(
        Verification complete. Stats: {
          "badContentCount": 0,
          "reclaimedCount": 0,
          "missingContent": 0,
          "verifiedContent": 0
        }
      ),
    ],
  ],
  "warn": Array [
    Array [
      "checkFilesPermission",
      "should be owner of {CWD}/cache/_cacache",
    ],
  ],
}
`

exports[`test/lib/commands/doctor.js TAP incorrect permissions > incorrect owner 1`] = `
Check                             [90m  [39mValue [90m  [39mRecommendation/Notes
npm ping                          [90m  [39mok    [90m  [39m
npm -v                            [90m  [39mok    [90m  [39mcurrent: v1.0.0, latest: v1.0.0
node -v                           [90m  [39mok    [90m  [39mcurrent: v1.0.0, recommended: v1.0.0
npm config get registry           [90m  [39mok    [90m  [39musing default registry (https://registry.npmjs.org/)
git executable in PATH            [90m  [39mok    [90m  [39m/path/to/git
global bin folder in PATH         [90m  [39mok    [90m  [39m{CWD}/global/bin
Perms check on cached files       [90m  [39mnot ok[90m  [39mCheck the permissions of files in {CWD}/cache (should be owned by current user)
Perms check on local node_modules [90m  [39mnot ok[90m  [39mCheck the permissions of files in {CWD}/prefix/node_modules (should be owned by current user)
Perms check on global node_modules[90m  [39mnot ok[90m  [39mCheck the permissions of files in {CWD}/global/node_modules
Perms check on local bin folder   [90m  [39mnot ok[90m  [39mCheck the permissions of files in {CWD}/prefix/node_modules/.bin
Perms check on global bin folder  [90m  [39mnot ok[90m  [39mCheck the permissions of files in {CWD}/global/bin
Verify cache contents             [90m  [39mok    [90m  [39mverified 0 tarballs
`

exports[`test/lib/commands/doctor.js TAP incorrect permissions > logs 1`] = `
Object {
  "error": Array [
    Array [
      "checkFilesPermission",
      "Missing permissions on {CWD}/cache (expect: readable)",
    ],
    Array [
      "checkFilesPermission",
      "Missing permissions on {CWD}/prefix/node_modules (expect: readable, writable)",
    ],
    Array [
      "checkFilesPermission",
      "Missing permissions on {CWD}/global/node_modules (expect: readable)",
    ],
    Array [
      "checkFilesPermission",
      "Missing permissions on {CWD}/prefix/node_modules/.bin (expect: readable, writable, executable)",
    ],
    Array [
      "checkFilesPermission",
      "Missing permissions on {CWD}/global/bin (expect: executable)",
    ],
  ],
  "info": Array [
    Array [
      "Running checkup",
    ],
    Array [
      "checkPing",
      "Pinging registry",
    ],
    Array [
      "getLatestNpmVersion",
      "Getting npm package information",
    ],
    Array [
      "getLatestNodejsVersion",
      "Getting Node.js release information",
    ],
    Array [
      "getGitPath",
      "Finding git in your PATH",
    ],
    Array [
      "getBinPath",
      "Finding npm global bin in your PATH",
    ],
    Array [
      "verifyCachedFiles",
      "Verifying the npm cache",
    ],
    Array [
      "verifyCachedFiles",
      String(
        Verification complete. Stats: {
          "badContentCount": 0,
          "reclaimedCount": 0,
          "missingContent": 0,
          "verifiedContent": 0
        }
      ),
    ],
  ],
  "warn": Array [],
}
`

exports[`test/lib/commands/doctor.js TAP missing git > logs 1`] = `
Object {
  "error": Array [],
  "info": Array [
    Array [
      "Running checkup",
    ],
    Array [
      "checkPing",
      "Pinging registry",
    ],
    Array [
      "getLatestNpmVersion",
      "Getting npm package information",
    ],
    Array [
      "getLatestNodejsVersion",
      "Getting Node.js release information",
    ],
    Array [
      "getGitPath",
      "Finding git in your PATH",
    ],
    Array [
      "getBinPath",
      "Finding npm global bin in your PATH",
    ],
    Array [
      "verifyCachedFiles",
      "Verifying the npm cache",
    ],
    Array [
      "verifyCachedFiles",
      String(
        Verification complete. Stats: {
          "badContentCount": 0,
          "reclaimedCount": 0,
          "missingContent": 0,
          "verifiedContent": 0
        }
      ),
    ],
  ],
  "warn": Array [
    Array [
      Error: test error,
    ],
  ],
}
`

exports[`test/lib/commands/doctor.js TAP missing git > missing git 1`] = `
Check                             [90m  [39mValue [90m  [39mRecommendation/Notes
npm ping                          [90m  [39mok    [90m  [39m
npm -v                            [90m  [39mok    [90m  [39mcurrent: v1.0.0, latest: v1.0.0
node -v                           [90m  [39mok    [90m  [39mcurrent: v1.0.0, recommended: v1.0.0
npm config get registry           [90m  [39mok    [90m  [39musing default registry (https://registry.npmjs.org/)
git executable in PATH            [90m  [39mnot ok[90m  [39mError: Install git and ensure it's in your PATH.
global bin folder in PATH         [90m  [39mok    [90m  [39m{CWD}/global/bin
Perms check on cached files       [90m  [39mok    [90m  [39m
Perms check on local node_modules [90m  [39mok    [90m  [39m
Perms check on global node_modules[90m  [39mok    [90m  [39m
Perms check on local bin folder   [90m  [39mok    [90m  [39m
Perms check on global bin folder  [90m  [39mok    [90m  [39m
Verify cache contents             [90m  [39mok    [90m  [39mverified 0 tarballs
`

exports[`test/lib/commands/doctor.js TAP missing global directories > logs 1`] = `
Object {
  "error": Array [],
  "info": Array [
    Array [
      "Running checkup",
    ],
    Array [
      "checkPing",
      "Pinging registry",
    ],
    Array [
      "getLatestNpmVersion",
      "Getting npm package information",
    ],
    Array [
      "getLatestNodejsVersion",
      "Getting Node.js release information",
    ],
    Array [
      "getGitPath",
      "Finding git in your PATH",
    ],
    Array [
      "getBinPath",
      "Finding npm global bin in your PATH",
    ],
    Array [
      "verifyCachedFiles",
      "Verifying the npm cache",
    ],
    Array [
      "verifyCachedFiles",
      String(
        Verification complete. Stats: {
          "badContentCount": 0,
          "reclaimedCount": 0,
          "missingContent": 0,
          "verifiedContent": 0
        }
      ),
    ],
  ],
  "warn": Array [
    Array [
      "checkFilesPermission",
      "error getting info for {CWD}/global/node_modules",
    ],
    Array [
      "checkFilesPermission",
      "error getting info for {CWD}/global/bin",
    ],
  ],
}
`

exports[`test/lib/commands/doctor.js TAP missing global directories > missing global directories 1`] = `
Check                             [90m  [39mValue [90m  [39mRecommendation/Notes
npm ping                          [90m  [39mok    [90m  [39m
npm -v                            [90m  [39mok    [90m  [39mcurrent: v1.0.0, latest: v1.0.0
node -v                           [90m  [39mok    [90m  [39mcurrent: v1.0.0, recommended: v1.0.0
npm config get registry           [90m  [39mok    [90m  [39musing default registry (https://registry.npmjs.org/)
git executable in PATH            [90m  [39mok    [90m  [39m/path/to/git
global bin folder in PATH         [90m  [39mok    [90m  [39m{CWD}/global/bin
Perms check on cached files       [90m  [39mok    [90m  [39m
Perms check on local node_modules [90m  [39mok    [90m  [39m
Perms check on global node_modules[90m  [39mnot ok[90m  [39mCheck the permissions of files in {CWD}/global/node_modules
Perms check on local bin folder   [90m  [39mok    [90m  [39m
Perms check on global bin folder  [90m  [39mnot ok[90m  [39mCheck the permissions of files in {CWD}/global/bin
Verify cache contents             [90m  [39mok    [90m  [39mverified 0 tarballs
`

exports[`test/lib/commands/doctor.js TAP missing local node_modules > logs 1`] = `
Object {
  "error": Array [],
  "info": Array [
    Array [
      "Running checkup",
    ],
    Array [
      "checkPing",
      "Pinging registry",
    ],
    Array [
      "getLatestNpmVersion",
      "Getting npm package information",
    ],
    Array [
      "getLatestNodejsVersion",
      "Getting Node.js release information",
    ],
    Array [
      "getGitPath",
      "Finding git in your PATH",
    ],
    Array [
      "getBinPath",
      "Finding npm global bin in your PATH",
    ],
    Array [
      "verifyCachedFiles",
      "Verifying the npm cache",
    ],
    Array [
      "verifyCachedFiles",
      String(
        Verification complete. Stats: {
          "badContentCount": 0,
          "reclaimedCount": 0,
          "missingContent": 0,
          "verifiedContent": 0
        }
      ),
    ],
  ],
  "warn": Array [],
}
`

exports[`test/lib/commands/doctor.js TAP missing local node_modules > missing local node_modules 1`] = `
Check                             [90m  [39mValue [90m  [39mRecommendation/Notes
npm ping                          [90m  [39mok    [90m  [39m
npm -v                            [90m  [39mok    [90m  [39mcurrent: v1.0.0, latest: v1.0.0
node -v                           [90m  [39mok    [90m  [39mcurrent: v1.0.0, recommended: v1.0.0
npm config get registry           [90m  [39mok    [90m  [39musing default registry (https://registry.npmjs.org/)
git executable in PATH            [90m  [39mok    [90m  [39m/path/to/git
global bin folder in PATH         [90m  [39mok    [90m  [39m{CWD}/global/bin
Perms check on cached files       [90m  [39mok    [90m  [39m
Perms check on local node_modules [90m  [39mok    [90m  [39m
Perms check on global node_modules[90m  [39mok    [90m  [39m
Perms check on local bin folder   [90m  [39mok    [90m  [39m
Perms check on global bin folder  [90m  [39mok    [90m  [39m
Verify cache contents             [90m  [39mok    [90m  [39mverified 0 tarballs
`

exports[`test/lib/commands/doctor.js TAP node out of date - current > logs 1`] = `
Object {
  "error": Array [],
  "info": Array [
    Array [
      "Running checkup",
    ],
    Array [
      "checkPing",
      "Pinging registry",
    ],
    Array [
      "getLatestNpmVersion",
      "Getting npm package information",
    ],
    Array [
      "getLatestNodejsVersion",
      "Getting Node.js release information",
    ],
    Array [
      "getGitPath",
      "Finding git in your PATH",
    ],
    Array [
      "getBinPath",
      "Finding npm global bin in your PATH",
    ],
    Array [
      "verifyCachedFiles",
      "Verifying the npm cache",
    ],
    Array [
      "verifyCachedFiles",
      String(
        Verification complete. Stats: {
          "badContentCount": 0,
          "reclaimedCount": 0,
          "missingContent": 0,
          "verifiedContent": 0
        }
      ),
    ],
  ],
  "warn": Array [],
}
`

exports[`test/lib/commands/doctor.js TAP node out of date - current > node is out of date 1`] = `
Check                             [90m  [39mValue [90m  [39mRecommendation/Notes
npm ping                          [90m  [39mok    [90m  [39m
npm -v                            [90m  [39mok    [90m  [39mcurrent: v1.0.0, latest: v1.0.0
node -v                           [90m  [39mnot ok[90m  [39mUse node v2.0.1 (current: v2.0.0)
npm config get registry           [90m  [39mok    [90m  [39musing default registry (https://registry.npmjs.org/)
git executable in PATH            [90m  [39mok    [90m  [39m/path/to/git
global bin folder in PATH         [90m  [39mok    [90m  [39m{CWD}/global/bin
Perms check on cached files       [90m  [39mok    [90m  [39m
Perms check on local node_modules [90m  [39mok    [90m  [39m
Perms check on global node_modules[90m  [39mok    [90m  [39m
Perms check on local bin folder   [90m  [39mok    [90m  [39m
Perms check on global bin folder  [90m  [39mok    [90m  [39m
Verify cache contents             [90m  [39mok    [90m  [39mverified 0 tarballs
`

exports[`test/lib/commands/doctor.js TAP node out of date - lts > logs 1`] = `
Object {
  "error": Array [],
  "info": Array [
    Array [
      "Running checkup",
    ],
    Array [
      "checkPing",
      "Pinging registry",
    ],
    Array [
      "getLatestNpmVersion",
      "Getting npm package information",
    ],
    Array [
      "getLatestNodejsVersion",
      "Getting Node.js release information",
    ],
    Array [
      "getGitPath",
      "Finding git in your PATH",
    ],
    Array [
      "getBinPath",
      "Finding npm global bin in your PATH",
    ],
    Array [
      "verifyCachedFiles",
      "Verifying the npm cache",
    ],
    Array [
      "verifyCachedFiles",
      String(
        Verification complete. Stats: {
          "badContentCount": 0,
          "reclaimedCount": 0,
          "missingContent": 0,
          "verifiedContent": 0
        }
      ),
    ],
  ],
  "warn": Array [],
}
`

exports[`test/lib/commands/doctor.js TAP node out of date - lts > node is out of date 1`] = `
Check                             [90m  [39mValue [90m  [39mRecommendation/Notes
npm ping                          [90m  [39mok    [90m  [39m
npm -v                            [90m  [39mok    [90m  [39mcurrent: v1.0.0, latest: v1.0.0
node -v                           [90m  [39mnot ok[90m  [39mUse node v1.0.0 (current: v0.0.1)
npm config get registry           [90m  [39mok    [90m  [39musing default registry (https://registry.npmjs.org/)
git executable in PATH            [90m  [39mok    [90m  [39m/path/to/git
global bin folder in PATH         [90m  [39mok    [90m  [39m{CWD}/global/bin
Perms check on cached files       [90m  [39mok    [90m  [39m
Perms check on local node_modules [90m  [39mok    [90m  [39m
Perms check on global node_modules[90m  [39mok    [90m  [39m
Perms check on local bin folder   [90m  [39mok    [90m  [39m
Perms check on global bin folder  [90m  [39mok    [90m  [39m
Verify cache contents             [90m  [39mok    [90m  [39mverified 0 tarballs
`

exports[`test/lib/commands/doctor.js TAP non-default registry > logs 1`] = `
Object {
  "error": Array [],
  "info": Array [
    Array [
      "Running checkup",
    ],
    Array [
      "checkPing",
      "Pinging registry",
    ],
    Array [
      "getLatestNpmVersion",
      "Getting npm package information",
    ],
    Array [
      "getLatestNodejsVersion",
      "Getting Node.js release information",
    ],
    Array [
      "getGitPath",
      "Finding git in your PATH",
    ],
    Array [
      "getBinPath",
      "Finding npm global bin in your PATH",
    ],
    Array [
      "verifyCachedFiles",
      "Verifying the npm cache",
    ],
    Array [
      "verifyCachedFiles",
      String(
        Verification complete. Stats: {
          "badContentCount": 0,
          "reclaimedCount": 0,
          "missingContent": 0,
          "verifiedContent": 0
        }
      ),
    ],
  ],
  "warn": Array [],
}
`

exports[`test/lib/commands/doctor.js TAP non-default registry > non default registry 1`] = `
Check                             [90m  [39mValue [90m  [39mRecommendation/Notes
npm ping                          [90m  [39mok    [90m  [39m
npm -v                            [90m  [39mok    [90m  [39mcurrent: v1.0.0, latest: v1.0.0
node -v                           [90m  [39mok    [90m  [39mcurrent: v1.0.0, recommended: v1.0.0
npm config get registry           [90m  [39mnot ok[90m  [39mTry \`npm config set registry=https://registry.npmjs.org/\`
git executable in PATH            [90m  [39mok    [90m  [39m/path/to/git
global bin folder in PATH         [90m  [39mok    [90m  [39m{CWD}/global/bin
Perms check on cached files       [90m  [39mok    [90m  [39m
Perms check on local node_modules [90m  [39mok    [90m  [39m
Perms check on global node_modules[90m  [39mok    [90m  [39m
Perms check on local bin folder   [90m  [39mok    [90m  [39m
Perms check on global bin folder  [90m  [39mok    [90m  [39m
Verify cache contents             [90m  [39mok    [90m  [39mverified 0 tarballs
`

exports[`test/lib/commands/doctor.js TAP npm out of date > logs 1`] = `
Object {
  "error": Array [],
  "info": Array [
    Array [
      "Running checkup",
    ],
    Array [
      "checkPing",
      "Pinging registry",
    ],
    Array [
      "getLatestNpmVersion",
      "Getting npm package information",
    ],
    Array [
      "getLatestNodejsVersion",
      "Getting Node.js release information",
    ],
    Array [
      "getGitPath",
      "Finding git in your PATH",
    ],
    Array [
      "getBinPath",
      "Finding npm global bin in your PATH",
    ],
    Array [
      "verifyCachedFiles",
      "Verifying the npm cache",
    ],
    Array [
      "verifyCachedFiles",
      String(
        Verification complete. Stats: {
          "badContentCount": 0,
          "reclaimedCount": 0,
          "missingContent": 0,
          "verifiedContent": 0
        }
      ),
    ],
  ],
  "warn": Array [],
}
`

exports[`test/lib/commands/doctor.js TAP npm out of date > npm is out of date 1`] = `
Check                             [90m  [39mValue [90m  [39mRecommendation/Notes
npm ping                          [90m  [39mok    [90m  [39m
npm -v                            [90m  [39mnot ok[90m  [39mUse npm v2.0.0
node -v                           [90m  [39mok    [90m  [39mcurrent: v1.0.0, recommended: v1.0.0
npm config get registry           [90m  [39mok    [90m  [39musing default registry (https://registry.npmjs.org/)
git executable in PATH            [90m  [39mok    [90m  [39m/path/to/git
global bin folder in PATH         [90m  [39mok    [90m  [39m{CWD}/global/bin
Perms check on cached files       [90m  [39mok    [90m  [39m
Perms check on local node_modules [90m  [39mok    [90m  [39m
Perms check on global node_modules[90m  [39mok    [90m  [39m
Perms check on local bin folder   [90m  [39mok    [90m  [39m
Perms check on global bin folder  [90m  [39mok    [90m  [39m
Verify cache contents             [90m  [39mok    [90m  [39mverified 0 tarballs
`

exports[`test/lib/commands/doctor.js TAP ping 404 > logs 1`] = `
Object {
  "error": Array [],
  "info": Array [
    Array [
      "Running checkup",
    ],
    Array [
      "checkPing",
      "Pinging registry",
    ],
    Array [
      "getLatestNpmVersion",
      "Getting npm package information",
    ],
    Array [
      "getLatestNodejsVersion",
      "Getting Node.js release information",
    ],
    Array [
      "getGitPath",
      "Finding git in your PATH",
    ],
    Array [
      "getBinPath",
      "Finding npm global bin in your PATH",
    ],
    Array [
      "verifyCachedFiles",
      "Verifying the npm cache",
    ],
    Array [
      "verifyCachedFiles",
      String(
        Verification complete. Stats: {
          "badContentCount": 0,
          "reclaimedCount": 0,
          "missingContent": 0,
          "verifiedContent": 0
        }
      ),
    ],
  ],
  "warn": Array [],
}
`

exports[`test/lib/commands/doctor.js TAP ping 404 > ping 404 1`] = `
Check                             [90m  [39mValue [90m  [39mRecommendation/Notes
npm ping                          [90m  [39mnot ok[90m  [39m404 404 Not Found - GET https://registry.npmjs.org/-/ping?write=true
npm -v                            [90m  [39mok    [90m  [39mcurrent: v1.0.0, latest: v1.0.0
node -v                           [90m  [39mok    [90m  [39mcurrent: v1.0.0, recommended: v1.0.0
npm config get registry           [90m  [39mok    [90m  [39musing default registry (https://registry.npmjs.org/)
git executable in PATH            [90m  [39mok    [90m  [39m/path/to/git
global bin folder in PATH         [90m  [39mok    [90m  [39m{CWD}/global/bin
Perms check on cached files       [90m  [39mok    [90m  [39m
Perms check on local node_modules [90m  [39mok    [90m  [39m
Perms check on global node_modules[90m  [39mok    [90m  [39m
Perms check on local bin folder   [90m  [39mok    [90m  [39m
Perms check on global bin folder  [90m  [39mok    [90m  [39m
Verify cache contents             [90m  [39mok    [90m  [39mverified 0 tarballs
`

exports[`test/lib/commands/doctor.js TAP ping 404 in color > logs 1`] = `
Object {
  "error": Array [],
  "info": Array [
    Array [
      "Running checkup",
    ],
    Array [
      "checkPing",
      "Pinging registry",
    ],
    Array [
      "getLatestNpmVersion",
      "Getting npm package information",
    ],
    Array [
      "getLatestNodejsVersion",
      "Getting Node.js release information",
    ],
    Array [
      "getGitPath",
      "Finding git in your PATH",
    ],
    Array [
      "getBinPath",
      "Finding npm global bin in your PATH",
    ],
    Array [
      "verifyCachedFiles",
      "Verifying the npm cache",
    ],
    Array [
      "verifyCachedFiles",
      String(
        Verification complete. Stats: {
          "badContentCount": 0,
          "reclaimedCount": 0,
          "missingContent": 0,
          "verifiedContent": 0
        }
      ),
    ],
  ],
  "warn": Array [],
}
`

exports[`test/lib/commands/doctor.js TAP ping 404 in color > ping 404 in color 1`] = `
[4mCheck[24m                             [90m  [39m[4mValue[24m [90m  [39m[4mRecommendation/Notes[24m
[31mnpm ping[39m                          [90m  [39m[31mnot ok[39m[90m  [39m[35m404 404 Not Found - GET https://registry.npmjs.org/-/ping?write=true[39m
npm -v                            [90m  [39m[32mok[39m    [90m  [39mcurrent: v1.0.0, latest: v1.0.0
node -v                           [90m  [39m[32mok[39m    [90m  [39mcurrent: v1.0.0, recommended: v1.0.0
npm config get registry           [90m  [39m[32mok[39m    [90m  [39musing default registry (https://registry.npmjs.org/)
git executable in PATH            [90m  [39m[32mok[39m    [90m  [39m/path/to/git
global bin folder in PATH         [90m  [39m[32mok[39m    [90m  [39m{CWD}/global/bin
Perms check on cached files       [90m  [39m[32mok[39m    [90m  [39m
Perms check on local node_modules [90m  [39m[32mok[39m    [90m  [39m
Perms check on global node_modules[90m  [39m[32mok[39m    [90m  [39m
Perms check on local bin folder   [90m  [39m[32mok[39m    [90m  [39m
Perms check on global bin folder  [90m  [39m[32mok[39m    [90m  [39m
Verify cache contents             [90m  [39m[32mok[39m    [90m  [39mverified 0 tarballs
`

exports[`test/lib/commands/doctor.js TAP ping exception with code > logs 1`] = `
Object {
  "error": Array [],
  "info": Array [
    Array [
      "Running checkup",
    ],
    Array [
      "checkPing",
      "Pinging registry",
    ],
    Array [
      "getLatestNpmVersion",
      "Getting npm package information",
    ],
    Array [
      "getLatestNodejsVersion",
      "Getting Node.js release information",
    ],
    Array [
      "getGitPath",
      "Finding git in your PATH",
    ],
    Array [
      "getBinPath",
      "Finding npm global bin in your PATH",
    ],
    Array [
      "verifyCachedFiles",
      "Verifying the npm cache",
    ],
    Array [
      "verifyCachedFiles",
      String(
        Verification complete. Stats: {
          "badContentCount": 0,
          "reclaimedCount": 0,
          "missingContent": 0,
          "verifiedContent": 0
        }
      ),
    ],
  ],
  "warn": Array [],
}
`

exports[`test/lib/commands/doctor.js TAP ping exception with code > ping failure 1`] = `
Check                             [90m  [39mValue [90m  [39mRecommendation/Notes
npm ping                          [90m  [39mnot ok[90m  [39mrequest to https://registry.npmjs.org/-/ping?write=true failed, reason: Test Error
npm -v                            [90m  [39mok    [90m  [39mcurrent: v1.0.0, latest: v1.0.0
node -v                           [90m  [39mok    [90m  [39mcurrent: v1.0.0, recommended: v1.0.0
npm config get registry           [90m  [39mok    [90m  [39musing default registry (https://registry.npmjs.org/)
git executable in PATH            [90m  [39mok    [90m  [39m/path/to/git
global bin folder in PATH         [90m  [39mok    [90m  [39m{CWD}/global/bin
Perms check on cached files       [90m  [39mok    [90m  [39m
Perms check on local node_modules [90m  [39mok    [90m  [39m
Perms check on global node_modules[90m  [39mok    [90m  [39m
Perms check on local bin folder   [90m  [39mok    [90m  [39m
Perms check on global bin folder  [90m  [39mok    [90m  [39m
Verify cache contents             [90m  [39mok    [90m  [39mverified 0 tarballs
`

exports[`test/lib/commands/doctor.js TAP ping exception without code > logs 1`] = `
Object {
  "error": Array [],
  "info": Array [
    Array [
      "Running checkup",
    ],
    Array [
      "checkPing",
      "Pinging registry",
    ],
    Array [
      "getLatestNpmVersion",
      "Getting npm package information",
    ],
    Array [
      "getLatestNodejsVersion",
      "Getting Node.js release information",
    ],
    Array [
      "getGitPath",
      "Finding git in your PATH",
    ],
    Array [
      "getBinPath",
      "Finding npm global bin in your PATH",
    ],
    Array [
      "verifyCachedFiles",
      "Verifying the npm cache",
    ],
    Array [
      "verifyCachedFiles",
      String(
        Verification complete. Stats: {
          "badContentCount": 0,
          "reclaimedCount": 0,
          "missingContent": 0,
          "verifiedContent": 0
        }
      ),
    ],
  ],
  "warn": Array [],
}
`

exports[`test/lib/commands/doctor.js TAP ping exception without code > ping failure 1`] = `
Check                             [90m  [39mValue [90m  [39mRecommendation/Notes
npm ping                          [90m  [39mnot ok[90m  [39mrequest to https://registry.npmjs.org/-/ping?write=true failed, reason: Test Error
npm -v                            [90m  [39mok    [90m  [39mcurrent: v1.0.0, latest: v1.0.0
node -v                           [90m  [39mok    [90m  [39mcurrent: v1.0.0, recommended: v1.0.0
npm config get registry           [90m  [39mok    [90m  [39musing default registry (https://registry.npmjs.org/)
git executable in PATH            [90m  [39mok    [90m  [39m/path/to/git
global bin folder in PATH         [90m  [39mok    [90m  [39m{CWD}/global/bin
Perms check on cached files       [90m  [39mok    [90m  [39m
Perms check on local node_modules [90m  [39mok    [90m  [39m
Perms check on global node_modules[90m  [39mok    [90m  [39m
Perms check on local bin folder   [90m  [39mok    [90m  [39m
Perms check on global bin folder  [90m  [39mok    [90m  [39m
Verify cache contents             [90m  [39mok    [90m  [39mverified 0 tarballs
`

exports[`test/lib/commands/doctor.js TAP silent errors > logs 1`] = `
Object {
  "error": Array [],
  "info": Array [
    Array [
      "Running checkup",
    ],
    Array [
      "checkPing",
      "Pinging registry",
    ],
  ],
  "warn": Array [],
}
`

exports[`test/lib/commands/doctor.js TAP silent errors > output 1`] = `

`

exports[`test/lib/commands/doctor.js TAP silent success > logs 1`] = `
Object {
  "error": Array [],
  "info": Array [
    Array [
      "Running checkup",
    ],
    Array [
      "checkPing",
      "Pinging registry",
    ],
    Array [
      "getLatestNpmVersion",
      "Getting npm package information",
    ],
    Array [
      "getLatestNodejsVersion",
      "Getting Node.js release information",
    ],
    Array [
      "getGitPath",
      "Finding git in your PATH",
    ],
    Array [
      "getBinPath",
      "Finding npm global bin in your PATH",
    ],
    Array [
      "verifyCachedFiles",
      "Verifying the npm cache",
    ],
    Array [
      "verifyCachedFiles",
      String(
        Verification complete. Stats: {
          "badContentCount": 0,
          "reclaimedCount": 0,
          "missingContent": 0,
          "verifiedContent": 0
        }
      ),
    ],
  ],
  "warn": Array [],
}
`

exports[`test/lib/commands/doctor.js TAP silent success > output 1`] = `

`

exports[`test/lib/commands/doctor.js TAP windows skips permissions checks > logs 1`] = `
Object {
  "error": Array [],
  "info": Array [
    Array [
      "Running checkup",
    ],
    Array [
      "checkPing",
      "Pinging registry",
    ],
    Array [
      "getLatestNpmVersion",
      "Getting npm package information",
    ],
    Array [
      "getLatestNodejsVersion",
      "Getting Node.js release information",
    ],
    Array [
      "getGitPath",
      "Finding git in your PATH",
    ],
    Array [
      "getBinPath",
      "Finding npm global bin in your PATH",
    ],
  ],
  "warn": Array [],
}
`

exports[`test/lib/commands/doctor.js TAP windows skips permissions checks > no permissions checks 1`] = `
Check                    [90m  [39mValue [90m  [39mRecommendation/Notes
npm ping                 [90m  [39mok    [90m  [39m
npm -v                   [90m  [39mok    [90m  [39mcurrent: v1.0.0, latest: v1.0.0
node -v                  [90m  [39mok    [90m  [39mcurrent: v1.0.0, recommended: v1.0.0
npm config get registry  [90m  [39mok    [90m  [39musing default registry (https://registry.npmjs.org/)
git executable in PATH   [90m  [39mok    [90m  [39m/path/to/git
global bin folder in PATH[90m  [39mok    [90m  [39m{CWD}/global
`
