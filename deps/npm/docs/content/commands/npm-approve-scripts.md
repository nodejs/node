---
title: npm-approve-scripts
section: 1
description: Approve install scripts for specific dependencies
---

### Synopsis

```bash
npm approve-scripts <pkg> [<pkg> ...]
npm approve-scripts --all
npm approve-scripts --allow-scripts-pending
```

Note: This command is unaware of workspaces.

### Description

Manages the `allowScripts` field in your project's `package.json`, which
records which of your dependencies are permitted to run install scripts
(`preinstall`, `install`, `postinstall`, and `prepare` for non-registry
sources). This command is the recommended way to maintain that field.

In the current release, this field is advisory: install scripts still run
by default, but installs print a list of packages whose scripts have not
been reviewed. A future release will block unreviewed install scripts.

This command only works inside a project that has a `package.json`. Running
it with `--global` (`-g`) fails with an `EGLOBAL` error, since global
installs (`npm install -g`) and one-off executions (`npm exec` / `npx`) have
no project `package.json` to write to. To allow install scripts in those
contexts, use the `--allow-scripts` flag at install time (for example
`npm install -g --allow-scripts=canvas,sharp`) or persist the setting with
`npm config set allow-scripts=canvas,sharp --location=user`.

There are three modes:

```bash
npm approve-scripts <pkg> [<pkg> ...]
npm approve-scripts --all
npm approve-scripts --allow-scripts-pending
```

`<pkg>` matches every installed version of that package. By default the
command writes pinned entries (`pkg@1.2.3`), which keep their approval
narrowed to the specific version you reviewed. Pass `--no-allow-scripts-pin` to write
name-only entries that allow any future version.

`--all` approves every package with unreviewed install scripts in one go.

`--allow-scripts-pending` is read-only: it lists every package whose install scripts
are not yet covered by `allowScripts`, without modifying `package.json`.

`approve-scripts` honours the asymmetric pin rule: if you re-approve a
package whose installed version has changed, the existing pin is rewritten
to track the new installed version. Multi-version statements
(`pkg@1 || 2`) are left alone, since they likely capture intent that
the command cannot infer. Existing `false` entries always win;
`approve-scripts` will not silently re-allow a package you previously
denied.

If a registry dependency has no `resolved` URL in your `package-lock.json`
(for example, an older lockfile or one written with
`omit-lockfile-registry-resolved`), npm cannot verify a trusted version for
it and cannot pin it: a `pkg@1.2.3` entry never matches, so the package
keeps appearing under `--allow-scripts-pending`. `approve-scripts` approves
these by name (`pkg: true`) and warns when it does. To restore pinning,
refresh the lockfile with `npm install`.

### Examples

```bash
# Approve all currently-installed install scripts after reviewing them
npm approve-scripts --all

# Approve specific packages, pinned to their installed version
npm approve-scripts canvas sharp

# Approve name-only (any version of this package is allowed)
npm approve-scripts --no-allow-scripts-pin canvas

# Preview which packages still need review
npm approve-scripts --allow-scripts-pending
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

* [npm deny-scripts](/commands/npm-deny-scripts)
* [npm install](/commands/npm-install)
* [npm rebuild](/commands/npm-rebuild)
* [package.json](/configuring-npm/package-json)
