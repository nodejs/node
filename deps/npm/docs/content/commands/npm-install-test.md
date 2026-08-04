---
title: npm-install-test
section: 1
description: Install package(s) and run tests
---

### Synopsis

```bash
npm install-test [<package-spec> ...]

alias: it
```

### Description

This command runs an `npm install` followed immediately by an `npm test`.
It takes exactly the same arguments as `npm install`.

### Configuration

#### `save`

* Default: `true` unless when using `npm update` where it defaults to `false`
* Type: Boolean

Save installed packages to a `package.json` file as dependencies.

When used with the `npm rm` command, removes the dependency from
`package.json`.

Will also prevent writing to `package-lock.json` if set to `false`.



#### `save-exact`

* Default: false
* Type: Boolean

Dependencies saved to package.json will be configured with an exact version
rather than using npm's default semver range operator.



#### `global`

* Default: false
* Type: Boolean

Operates in "global" mode, so that packages are installed into the `prefix`
folder instead of the current working directory. See
[folders](/configuring-npm/folders) for more on the differences in behavior.

* packages are installed into the `{prefix}/lib/node_modules` folder, instead
  of the current working directory.
* bin files are linked to `{prefix}/bin`
* man pages are linked to `{prefix}/share/man`



#### `install-strategy`

* Default: "hoisted"
* Type: "hoisted", "nested", "shallow", or "linked"

Sets the strategy for installing packages in node_modules. hoisted
(default): Install non-duplicated in top-level, and duplicated as necessary
within directory structure. nested: (formerly --legacy-bundling) install in
place, no hoisting. shallow (formerly --global-style) only install direct
deps at top-level. linked: install in node_modules/.store, link in place,
unhoisted.

We recommend that package authors use `--install-strategy=linked` during
development to catch undeclared ("phantom") dependencies before publishing:
the isolated layout only exposes a package's declared dependencies, so an
`import` of a package that was never added to `package.json` can fail
instead of resolving by accident and shipping broken. See [Catching
undeclared ("phantom")
dependencies](/using-npm/developers#catching-undeclared-phantom-dependencies).



#### `legacy-bundling`

* Default: false
* Type: Boolean
* DEPRECATED: This option has been deprecated in favor of
  `--install-strategy=nested`

Instead of hoisting package installs in `node_modules`, install packages in
the same manner that they are depended on. This may cause very deep
directory structures and duplicate package installs as there is no
de-duplicating. Sets `--install-strategy=nested`.



#### `global-style`

* Default: false
* Type: Boolean
* DEPRECATED: This option has been deprecated in favor of
  `--install-strategy=shallow`

Only install direct dependencies in the top level `node_modules`, but hoist
on deeper dependencies. Sets `--install-strategy=shallow`.



#### `omit`

* Default: 'dev' if the `NODE_ENV` environment variable is set to
  'production'; otherwise, empty.
* Type: "dev", "optional", or "peer" (can be set multiple times)

Dependency types to omit from the installation tree on disk.

Note that these dependencies _are_ still resolved and added to the
`package-lock.json` file. They are just not physically installed on disk.

If a package type appears in both the `--include` and `--omit` lists, then
it will be included.

If the resulting omit list includes `'dev'`, then the `NODE_ENV` environment
variable will be set to `'production'` for all lifecycle scripts.



#### `include`

* Default:
* Type: "prod", "dev", "optional", or "peer" (can be set multiple times)

Option that allows for defining which types of dependencies to install.

This is the inverse of `--omit=<type>`.

Dependency types specified in `--include` will not be omitted, regardless of
the order in which omit/include are specified on the command-line.



#### `strict-peer-deps`

* Default: false
* Type: Boolean

If set to `true`, and `--legacy-peer-deps` is not set, then _any_
conflicting `peerDependencies` will be treated as an install failure, even
if npm could reasonably guess the appropriate resolution based on non-peer
dependency relationships.

By default, conflicting `peerDependencies` deep in the dependency graph will
be resolved using the nearest non-peer dependency specification, even if
doing so will result in some packages receiving a peer dependency outside
the range set in their package's `peerDependencies` object.

When such an override is performed, a warning is printed, explaining the
conflict and the packages involved. If `--strict-peer-deps` is set, then
this warning is treated as a failure.



#### `prefer-dedupe`

* Default: false
* Type: Boolean

Prefer to deduplicate packages if possible, rather than choosing a newer
version of a dependency.



#### `package-lock`

* Default: true
* Type: Boolean

If set to false, then ignore `package-lock.json` files when installing. This
will also prevent _writing_ `package-lock.json` if `save` is true.



#### `package-lock-only`

* Default: false
* Type: Boolean

If set to true, the current operation will only use the `package-lock.json`,
ignoring `node_modules`.

For `update` this means only the `package-lock.json` will be updated,
instead of checking `node_modules` and downloading dependencies.

For `list` this means the output will be based on the tree described by the
`package-lock.json`, rather than the contents of `node_modules`.



#### `foreground-scripts`

* Default: `false` unless when using `npm pack` or `npm publish` where it
  defaults to `true`
* Type: Boolean

Run all build scripts (ie, `preinstall`, `install`, and `postinstall`)
scripts for installed packages in the foreground process, sharing standard
input, output, and error with the main npm process.

Note that this will generally make installs run slower, and be much noisier,
but can be useful for debugging.



#### `ignore-scripts`

* Default: false
* Type: Boolean

If true, npm does not run scripts specified in package.json files.

Note that commands explicitly intended to run a particular script, such as
`npm start`, `npm stop`, `npm restart`, `npm test`, and `npm run` will still
run their intended script if `ignore-scripts` is set, but they will *not*
run any pre- or post-scripts.

Setting `ignore-scripts` also disables `.npm-extension` execution, as if
`ignore-extension` were set.



#### `allow-directory`

* Default: "all"
* Type: "all", "none", or "root"

Limits the ability for npm to install dependencies from directories. That
is, dependencies that point to a directory instead of a version or semver
range. Please note that this could leave your tree incomplete and some
packages may not function as intended or designed. Changing this setting
will not remove dependencies that are already installed.

`all` allows any directories to be installed. `none` prevents any
directories from being installed. `root` only allows directories defined in
your project's package.json to be installed. Also allows directory
dependencies to be used for other commands like `npm view`



#### `allow-file`

* Default: "all"
* Type: "all", "none", or "root"

Limits the ability for npm to install dependencies from tarball files. That
is, dependencies that point to a local tarball file instead of a version or
semver range. Please note that this could leave your tree incomplete and
some packages may not function as intended or designed. Changing this
setting will not remove dependencies that are already installed.

`all` allows any tarball file to be installed. `none` prevents any tarball
file from being installed. `root` only allows tarball files defined in your
project's package.json to be installed. Also allows tarball file
dependencies to be used for other commands like `npm view`



#### `allow-git`

* Default: "none"
* Type: "all", "none", or "root"

Limits the ability for npm to fetch dependencies from git references. That
is, dependencies that point to a git repo instead of a version or semver
range. Please note that this could leave your tree incomplete and some
packages may not function as intended or designed. Changing this setting
will not remove dependencies that are already installed.

As of npm 12 the default is `none`. Git dependencies run `git` against a
remote repo and may install configuration the project does not control. Opt
in explicitly per project (in `.npmrc`) or per command (on the CLI) when you
need git deps.

`all` allows any git dependencies to be fetched and installed. `none`
prevents any git dependencies from being fetched and installed. `root` only
allows git dependencies defined in your project's package.json to be fetched
and installed. Also allows git dependencies to be fetched for other commands
like `npm view`



#### `allow-remote`

* Default: "none"
* Type: "all", "none", or "root"

Limits the ability for npm to fetch dependencies from urls. That is,
dependencies that point to a tarball url instead of a version or semver
range. Please note that this could leave your tree incomplete and some
packages may not function as intended or designed. Changing this setting
will not remove dependencies that are already installed.

As of npm 12 the default is `none`. Tarballs that share a hostname with the
configured registry (the typical case for the npm registry, GitHub Packages,
and most private registries) are still installed normally. If your registry
serves tarballs from a different host, set `replace-registry-host` or
override this setting. Opt in explicitly per project (in `.npmrc`) or per
command (on the CLI) when you intentionally install from a URL.

`all` allows any url to be installed. `none` prevents any url from being
installed. `root` only allows urls defined in your project's package.json to
be installed. Also allows url dependencies to be used for other commands
like `npm view`



#### `allow-scripts`

* Default: ""
* Type: String (can be set multiple times)

Comma-separated list of packages whose install-time lifecycle scripts
(`preinstall`, `install`, `postinstall`, and `prepare` for non-registry
dependencies) are allowed to run.

This setting is intended for one-off and global contexts: `npm exec`, `npx`,
and `npm install -g`, where no project `package.json` is involved. For
team-wide policy in a project, use the `allowScripts` field in
`package.json` (which also supports explicit denials), or configure it in
`.npmrc`. Passing `--allow-scripts` on the command line during a
project-scoped `npm install`, `ci`, `update`, or `rebuild` is an error.

Each name is matched against a dependency's resolved identity, not against
the package's self-reported name. `--ignore-scripts` and
`--dangerously-allow-all-scripts` both override this setting.



#### `strict-allow-scripts`

* Default: false
* Type: Boolean

If `true`, turn the install-script policy from a warning into a hard error:
any dependency with install scripts that is not covered by `allowScripts`
will fail the install instead of being blocked with a warning.

Dependencies explicitly denied with `false` in `allowScripts` are always
silently skipped; this setting only affects unreviewed entries (packages
with install scripts that are neither approved nor denied).
`--ignore-scripts` and `--dangerously-allow-all-scripts` both override this
setting.

Optional dependencies that cannot be installed on the current platform or
engine (a non-matching `os`, `cpu`, or `libc`) are not flagged, because
their install scripts never run.



#### `dangerously-allow-all-scripts`

* Default: false
* Type: Boolean

If `true`, bypass the `allowScripts` policy entirely and run every
dependency install script regardless of whether it was approved or denied.
Intended as a migration escape hatch only; its use is strongly discouraged.
`--ignore-scripts` still takes precedence over this setting.



#### `audit`

* Default: true
* Type: Boolean

When "true" submit audit reports alongside the current npm command to the
default registry and all registries configured for scopes. See the
documentation for [`npm audit`](/commands/npm-audit) for details on what is
submitted.



#### `before`

* Default: null
* Type: null or Date

If passed to `npm install`, will rebuild the npm tree such that only
versions that were available **on or before** the given date are installed.
If there are no versions available for the current set of dependencies, the
command will error.

If the requested version is a `dist-tag` and the given tag does not pass the
`--before` filter, the most recent version less than or equal to that tag
will be used. For example, `foo@latest` might install `foo@1.2` even though
`latest` is `2.0`.

If `before` and `min-release-age` are both set in the same source, `before`
wins (an explicit absolute date overrides a relative window). Across
sources, the standard precedence applies (cli > env > project > user >
global), so a higher-priority source can always relax or override a
lower-priority one.

As with `min-release-age`, when this cutoff blocks a fix that `npm audit
fix` would install, npm keeps the vulnerable version, warns, and exits with
a non-zero code.

Packages whose names match `min-release-age-exclude` are exempt from this
filter.



#### `min-release-age`

* Default: null
* Type: null or Number

If set, npm will build the npm tree such that only versions that were
available more than the given number of days ago will be installed. If there
are no versions available for the current set of dependencies, the command
will error.

This flag is a complement to `before`, which accepts an exact date instead
of a relative number of days. The two may coexist (e.g. `min-release-age` in
your `.npmrc` is preserved when npm internally spawns a sub-process with
`--before` while preparing a `git:` or `github:` dependency); when both
apply, `before` wins within a single source and across sources the standard
precedence rules apply.

When this window stops `npm audit fix` from installing a patched version
(because the fix was published too recently), npm keeps the package at its
vulnerable version, warns that the fix was blocked, and exits with a
non-zero code. To install the fix, add the package to
`min-release-age-exclude`, or relax `min-release-age` or `before`.

Packages whose names match `min-release-age-exclude` are exempt from this
filter.

This value is not exported to the environment for child processes.

#### `min-release-age-exclude`

* Default:
* Type: String (can be set multiple times)

A list of package names or `minimatch` glob patterns that are exempt from
the `min-release-age` (and `before`) filter. A matching package can always
resolve to its newest version, even when a release-age window is set.

For example, to apply a release-age window to third-party dependencies while
letting internally maintained packages update immediately:

```
min-release-age=7
min-release-age-exclude[]=@myorg/*
min-release-age-exclude[]=my-internal-pkg
```

Only the named package is exempt; its own dependencies still follow the
release-age policy unless they also match a pattern. Patterns match against
the package name, so `@myorg/*` matches `@myorg/shared-utils`.

Excluding a package does not change which registry it is fetched from. You
should own your private scope on the public registry so that nobody else can
publish a package with the same name.

This value is not exported to the environment for child processes.

#### `bin-links`

* Default: true
* Type: Boolean

Tells npm to create symlinks (or `.cmd` shims on Windows) for package
executables.

Set to false to have it not do this. This can be used to work around the
fact that some file systems don't support symlinks, even on ostensibly Unix
systems.



#### `fund`

* Default: true
* Type: Boolean

When "true" displays the message at the end of each `npm install`
acknowledging the number of dependencies looking for funding. See [`npm
fund`](/commands/npm-fund) for details.



#### `dry-run`

* Default: false
* Type: Boolean

Indicates that you don't want npm to make any changes and that it should
only report what it would have done. This can be passed into any of the
commands that modify your local installation, eg, `install`, `update`,
`dedupe`, `uninstall`, as well as `pack` and `publish`.

Note: This is NOT honored by other network related commands, eg `dist-tags`,
`owner`, etc.



#### `cpu`

* Default: null
* Type: null or String

Override CPU architecture of native modules to install. Acceptable values
are same as `cpu` field of package.json, which comes from `process.arch`.



#### `os`

* Default: null
* Type: null or String

Override OS of native modules to install. Acceptable values are same as `os`
field of package.json, which comes from `process.platform`.



#### `libc`

* Default: null
* Type: null or String

Override libc of native modules to install. Acceptable values are same as
`libc` field of package.json



#### `workspace`

* Default:
* Type: String (can be set multiple times)

Enable running a command in the context of the configured workspaces of the
current project while filtering by running only the workspaces defined by
this configuration option.

Valid values for the `workspace` config are either:

* Workspace names
* Path to a workspace directory
* Path to a parent workspace directory (will result in selecting all
  workspaces within that folder)

When set for the `npm init` command, this may be set to the folder of a
workspace which does not yet exist, to create the folder and set it up as a
brand new workspace within the project.

This value is not exported to the environment for child processes.

#### `workspaces`

* Default: null
* Type: null or Boolean

Set to true to run the command in the context of **all** configured
workspaces.

Explicitly setting this to false will cause commands like `install` to
ignore workspaces altogether. When not set explicitly:

- Commands that operate on the `node_modules` tree (install, update, etc.)
will link workspaces into the `node_modules` folder. - Commands that do
other things (test, exec, publish, etc.) will operate on the root project,
_unless_ one or more workspaces are specified in the `workspace` config.

This value is not exported to the environment for child processes.

#### `include-workspace-root`

* Default: false
* Type: Boolean

Include the workspace root when workspaces are enabled for a command.

When false, specifying individual workspaces via the `workspace` config, or
all workspaces via the `workspaces` flag, will cause npm to operate only on
the specified workspaces, and not on the root project.

This value is not exported to the environment for child processes.

#### `install-links`

* Default: false
* Type: Boolean

When set file: protocol dependencies will be packed and installed as regular
dependencies instead of creating a symlink. This option has no effect on
workspaces.



### See Also

* [npm install](/commands/npm-install)
* [npm install-ci-test](/commands/npm-install-ci-test)
* [npm test](/commands/npm-test)
