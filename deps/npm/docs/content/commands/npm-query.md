---
title: npm-query
section: 1
description: Dependency selector query
---

### Synopsis

```bash
npm query <selector>
```

### Description

The `npm query` command allows for usage of css selectors in order to retrieve an array of dependency objects.

### Piping npm query to other commands

```bash
# find all dependencies with postinstall scripts & uninstall them
npm query ":attr(scripts, [postinstall])" | jq 'map(.name)|join("\n")' -r | xargs -I {} npm uninstall {}

# find all git dependencies & explain who requires them
npm query ":type(git)" | jq 'map(.name)' | xargs -I {} npm why {}
```

### Extended Use Cases & Queries

```stylus
// all deps
*

// all direct deps
:root > *

// direct production deps
:root > .prod

// direct development deps
:root > .dev

// any peer dep of a direct deps
:root > * > .peer

// any workspace dep
.workspace

// all workspaces that depend on another workspace
.workspace > .workspace

// all workspaces that have peer deps
.workspace:has(.peer)

// any dep named "lodash"
// equivalent to [name="lodash"]
#lodash

// any deps named "lodash" & within semver range ^"1.2.3"
#lodash@^1.2.3
// equivalent to...
[name="lodash"]:semver(^1.2.3)

// get the hoisted node for a given semver range
#lodash@^1.2.3:not(:deduped)

// querying deps with a specific version
#lodash@2.1.5
// equivalent to...
[name="lodash"][version="2.1.5"]

// has any deps
:has(*)

// deps with no other deps (ie. "leaf" nodes)
:empty

// manually querying git dependencies
[repository^=github:],
[repository^=git:],
[repository^=https://github.com],
[repository^=http://github.com],
[repository^=https://github.com],
[repository^=+git:...]

// querying for all git dependencies
:type(git)

// get production dependencies that aren't also dev deps
.prod:not(.dev)

// get dependencies with specific licenses
[license=MIT], [license=ISC]

// find all packages that have @ruyadorno as a contributor
:attr(contributors, [email=ruyadorno@github.com])
```

### Example Response Output

- an array of dependency objects is returned which can contain multiple copies of the same package which may or may not have been linked or deduped

```json
[
  {
    "name": "",
    "version": "",
    "description": "",
    "homepage": "",
    "bugs": {},
    "author": {},
    "license": {},
    "funding": {},
    "files": [],
    "main": "",
    "browser": "",
    "bin": {},
    "man": [],
    "directories": {},
    "repository": {},
    "scripts": {},
    "config": {},
    "dependencies": {},
    "devDependencies": {},
    "optionalDependencies": {},
    "bundledDependencies": {},
    "peerDependencies": {},
    "peerDependenciesMeta": {},
    "engines": {},
    "os": [],
    "cpu": [],
    "workspaces": {},
    "keywords": [],
    ...
  },
  ...
```

### Expecting a certain number of results

One common use of `npm query` is to make sure there is only one version of a certain dependency in your tree.
This is especially common for ecosystems like that rely on `typescript` where having state split across two different but identically-named packages causes bugs.
You can use the `--expect-results` or `--expect-result-count` in your setup to ensure that npm will exit with an exit code if your tree doesn't look like you want it to.


```sh
$ npm query '#react' --expect-result-count=1
```

Perhaps you want to quickly check if there are any production dependencies that could be updated:

```sh
$ npm query ':root>:outdated(in-range).prod' --no-expect-results
```

### Package lock only mode

If package-lock-only is enabled, only the information in the package lock (or shrinkwrap) is loaded.
This means that information from the package.json files of your dependencies will not be included in the result set (e.g. description, homepage, engines).

### Configuration

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

#### `package-lock-only`

* Default: false
* Type: Boolean

If set to true, the current operation will only use the `package-lock.json`,
ignoring `node_modules`.

For `update` this means only the `package-lock.json` will be updated,
instead of checking `node_modules` and downloading dependencies.

For `list` this means the output will be based on the tree described by the
`package-lock.json`, rather than the contents of `node_modules`.



#### `expect-results`

* Default: null
* Type: null or Boolean

Tells npm whether or not to expect results from the command. Can be either
true (expect some results) or false (expect no results).

This config cannot be used with: `expect-result-count`

#### `expect-result-count`

* Default: null
* Type: null or Number

Tells to expect a specific number of results from the command.

This config cannot be used with: `expect-results`

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
## See Also

* [dependency selectors](/using-npm/dependency-selectors)
