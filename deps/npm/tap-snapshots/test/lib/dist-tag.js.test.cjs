/* IMPORTANT
 * This snapshot file is auto-generated, but designed for humans.
 * It should be checked into source control and tracked carefully.
 * Re-generate by setting TAP_SNAPSHOT=1 and running tests.
 * Make sure to inspect the output below.  Do not ignore changes!
 */
'use strict'
exports[`test/lib/dist-tag.js TAP add missing args > should exit usage error message 1`] = `
Error:
Usage: npm dist-tag

Modify package distribution tags

Usage:
npm dist-tag add <pkg>@<version> [<tag>]
npm dist-tag rm <pkg> <tag>
npm dist-tag ls [<pkg>]

Options:
[-w|--workspace <workspace-name> [-w|--workspace <workspace-name> ...]]
[-ws|--workspaces]

alias: dist-tags

Run "npm help dist-tag" for more info {
  "code": "EUSAGE",
}
`

exports[`test/lib/dist-tag.js TAP add missing pkg name > should exit usage error message 1`] = `
Error:
Usage: npm dist-tag

Modify package distribution tags

Usage:
npm dist-tag add <pkg>@<version> [<tag>]
npm dist-tag rm <pkg> <tag>
npm dist-tag ls [<pkg>]

Options:
[-w|--workspace <workspace-name> [-w|--workspace <workspace-name> ...]]
[-ws|--workspaces]

alias: dist-tags

Run "npm help dist-tag" for more info {
  "code": "EUSAGE",
}
`

exports[`test/lib/dist-tag.js TAP add new tag > should return success msg 1`] = `
+c: @scoped/another@7.7.7
`

exports[`test/lib/dist-tag.js TAP add using valid semver range as name > should return success msg 1`] = `
dist-tag add 1.0.0 to @scoped/another@7.7.7 

`

exports[`test/lib/dist-tag.js TAP borked cmd usage > should show usage error 1`] = `
Error:
Usage: npm dist-tag

Modify package distribution tags

Usage:
npm dist-tag add <pkg>@<version> [<tag>]
npm dist-tag rm <pkg> <tag>
npm dist-tag ls [<pkg>]

Options:
[-w|--workspace <workspace-name> [-w|--workspace <workspace-name> ...]]
[-ws|--workspaces]

alias: dist-tags

Run "npm help dist-tag" for more info {
  "code": "EUSAGE",
}
`

exports[`test/lib/dist-tag.js TAP ls global > should throw basic usage 1`] = `
Error:
Usage: npm dist-tag

Modify package distribution tags

Usage:
npm dist-tag add <pkg>@<version> [<tag>]
npm dist-tag rm <pkg> <tag>
npm dist-tag ls [<pkg>]

Options:
[-w|--workspace <workspace-name> [-w|--workspace <workspace-name> ...]]
[-ws|--workspaces]

alias: dist-tags

Run "npm help dist-tag" for more info {
  "code": "EUSAGE",
}
`

exports[`test/lib/dist-tag.js TAP ls in current package > should list available tags for current package 1`] = `
a: 0.0.1
b: 0.5.0
latest: 1.0.0
`

exports[`test/lib/dist-tag.js TAP ls on missing name in current package > should throw usage error message 1`] = `
Error:
Usage: npm dist-tag

Modify package distribution tags

Usage:
npm dist-tag add <pkg>@<version> [<tag>]
npm dist-tag rm <pkg> <tag>
npm dist-tag ls [<pkg>]

Options:
[-w|--workspace <workspace-name> [-w|--workspace <workspace-name> ...]]
[-ws|--workspaces]

alias: dist-tags

Run "npm help dist-tag" for more info {
  "code": "EUSAGE",
}
`

exports[`test/lib/dist-tag.js TAP ls on missing package > should log no dist-tag found msg 1`] = `
dist-tag ls Couldn't get dist-tag data for foo@latest 

`

exports[`test/lib/dist-tag.js TAP ls on missing package > should throw error message 1`] = `
Error: No dist-tags found for foo
`

exports[`test/lib/dist-tag.js TAP ls on named package > should list tags for the specified package 1`] = `
a: 0.0.2
b: 0.6.0
latest: 2.0.0
`

exports[`test/lib/dist-tag.js TAP no args in current package > should default to listing available tags for current package 1`] = `
a: 0.0.1
b: 0.5.0
latest: 1.0.0
`

exports[`test/lib/dist-tag.js TAP only named package arg > should default to listing tags for the specified package 1`] = `
a: 0.0.2
b: 0.6.0
latest: 2.0.0
`

exports[`test/lib/dist-tag.js TAP remove existing tag > should log remove info 1`] = `
dist-tag del c from @scoped/another 

`

exports[`test/lib/dist-tag.js TAP remove existing tag > should return success msg 1`] = `
-c: @scoped/another@7.7.7
`

exports[`test/lib/dist-tag.js TAP remove missing pkg name > should exit usage error message 1`] = `
Error:
Usage: npm dist-tag

Modify package distribution tags

Usage:
npm dist-tag add <pkg>@<version> [<tag>]
npm dist-tag rm <pkg> <tag>
npm dist-tag ls [<pkg>]

Options:
[-w|--workspace <workspace-name> [-w|--workspace <workspace-name> ...]]
[-ws|--workspaces]

alias: dist-tags

Run "npm help dist-tag" for more info {
  "code": "EUSAGE",
}
`

exports[`test/lib/dist-tag.js TAP remove non-existing tag > should log error msg 1`] = `
dist-tag del nonexistent from @scoped/another 
dist-tag del nonexistent is not a dist-tag on @scoped/another 

`

exports[`test/lib/dist-tag.js TAP set existing version > should log warn msg 1`] = `
dist-tag add b to @scoped/another@0.6.0 
dist-tag add b is already set to version 0.6.0 

`

exports[`test/lib/dist-tag.js TAP workspaces no args > printed the expected output 1`] = `
workspace-a:
latest-a: 1.0.0
latest: 1.0.0
workspace-b:
latest-b: 2.0.0
latest: 2.0.0
workspace-c:
latest-c: 3.0.0
latest: 3.0.0
`

exports[`test/lib/dist-tag.js TAP workspaces no args, one failing workspace sets exitCode to 1 > printed the expected output 1`] = `
workspace-a:
latest-a: 1.0.0
latest: 1.0.0
workspace-b:
latest-b: 2.0.0
latest: 2.0.0
workspace-c:
latest-c: 3.0.0
latest: 3.0.0
workspace-d:
`

exports[`test/lib/dist-tag.js TAP workspaces no args, one workspace > printed the expected output 1`] = `
workspace-a:
latest-a: 1.0.0
latest: 1.0.0
`

exports[`test/lib/dist-tag.js TAP workspaces one arg -- . > printed the expected output 1`] = `
workspace-a:
latest-a: 1.0.0
latest: 1.0.0
workspace-b:
latest-b: 2.0.0
latest: 2.0.0
workspace-c:
latest-c: 3.0.0
latest: 3.0.0
`

exports[`test/lib/dist-tag.js TAP workspaces one arg -- .@1, ignores version spec > printed the expected output 1`] = `
workspace-a:
latest-a: 1.0.0
latest: 1.0.0
workspace-b:
latest-b: 2.0.0
latest: 2.0.0
workspace-c:
latest-c: 3.0.0
latest: 3.0.0
`

exports[`test/lib/dist-tag.js TAP workspaces one arg -- list > printed the expected output 1`] = `
workspace-a:
latest-a: 1.0.0
latest: 1.0.0
workspace-b:
latest-b: 2.0.0
latest: 2.0.0
workspace-c:
latest-c: 3.0.0
latest: 3.0.0
`

exports[`test/lib/dist-tag.js TAP workspaces two args -- list, . > printed the expected output 1`] = `
workspace-a:
latest-a: 1.0.0
latest: 1.0.0
workspace-b:
latest-b: 2.0.0
latest: 2.0.0
workspace-c:
latest-c: 3.0.0
latest: 3.0.0
`

exports[`test/lib/dist-tag.js TAP workspaces two args -- list, .@1, ignores version spec > printed the expected output 1`] = `
workspace-a:
latest-a: 1.0.0
latest: 1.0.0
workspace-b:
latest-b: 2.0.0
latest: 2.0.0
workspace-c:
latest-c: 3.0.0
latest: 3.0.0
`

exports[`test/lib/dist-tag.js TAP workspaces two args -- list, @scoped/pkg, logs a warning and ignores workspaces > printed the expected output 1`] = `
a: 0.0.1
b: 0.5.0
latest: 1.0.0
`
