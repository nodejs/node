---
title: npm-deny-scripts
section: 1
description: Deny install scripts for specific dependencies
---

### Synopsis

```bash
npm deny-scripts <pkg> [<pkg> ...]
npm deny-scripts --all
```

Note: This command is unaware of workspaces.

### Description

The companion command to [`npm approve-scripts`](/commands/npm-approve-scripts).
Writes `false` entries into the `allowScripts` field of your project's
`package.json`, recording that a dependency must not run install scripts
even if a future version would otherwise be eligible.

Dependency install scripts are blocked by default. Adding a `false`
entry with `deny-scripts` makes the denial explicit (so it survives
`npm approve-scripts --all`) and excludes the package from any future
`--allow-scripts-pending` review prompts.

```bash
npm deny-scripts <pkg> [<pkg> ...]
npm deny-scripts --all
```

`<pkg>` matches every installed version of that package. Denies are always
written name-only (`"pkg": false`), regardless of `--allow-scripts-pin`. Pinning a deny
to a specific version would silently re-allow scripts for any other version
of the same package, which defeats the purpose; the command picks the
safer default for you.

`--all` denies every package with unreviewed install scripts.

If a `true` (pinned or name-only) entry exists for a package and you then
deny it, the existing allow entries are removed so the name-only deny is
unambiguous.

### Examples

```bash
# Deny a specific package outright
npm deny-scripts telemetry-pkg

# Deny everything that has install scripts and isn't already approved
npm deny-scripts --all
```

### Configuration

#### `all`

* Default: false
* Type: Boolean

Show or act on all packages, not just the ones your project directly depends
on. For `npm outdated` and `npm ls` this lists every outdated or installed
package. For `npm approve-scripts` and `npm deny-scripts` it selects every
package with pending install scripts.



#### `allow-scripts-pending`

* Default: false
* Type: Boolean

List packages with install scripts that are not yet covered by the
`allowScripts` policy, without modifying `package.json`. Only meaningful for
`npm approve-scripts`.



#### `allow-scripts-pin`

* Default: true
* Type: Boolean

Write pinned (`pkg@version`) entries when approving install scripts. Set to
`false` to write name-only entries that allow any version. Has no effect on
`npm deny-scripts`, which always writes name-only entries regardless of this
setting.



#### `json`

* Default: false
* Type: Boolean

Whether or not to output JSON data, rather than the normal output.

* In `npm pkg set` it enables parsing set values with JSON.parse() before
  saving them to your `package.json`.

Not supported by all npm commands.



### See Also

* [npm approve-scripts](/commands/npm-approve-scripts)
* [npm install](/commands/npm-install)
* [package.json](/configuring-npm/package-json)
