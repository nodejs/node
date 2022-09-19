---
title: package-spec
section: 7
description: Package name specifier
---


### Description

Commands like `npm install` and the dependency sections in the
`package.json` use a package name specifier.  This can be many different
things that all refer to a "package".  Examples include a package name,
git url, tarball, or local directory.  These will generally be referred
to as `<package-spec>` in the help output for the npm commands that use
this package name specifier.

### Package name

* `[<@scope>/]<pkg>`
* `[<@scope>/]<pkg>@<tag>`
* `[<@scope>/]<pkg>@<version>`
* `[<@scope>/]<pkg>@<version range>`

Refers to a package by name, with or without a scope, and optionally
tag, version, or version range.  This is typically used in combination
with the [registry](/using-npm/config#registry) config to refer to a
package in a registry.

Examples:
* `npm`
* `@npmcli/arborist`
* `@npmcli/arborist@latest`
* `npm@6.13.1`
* `npm@^4.0.0`

### Aliases

* `<alias>@npm:<name>`

Primarily used by commands like `npm install` and in the dependency
sections in the `package.json`, this refers to a package by an alias.
The `<alias>` is the name of the package as it is reified in the
`node_modules` folder, and the `<name>` refers to a package name as
found in the configured registry.

See `Package name` above for more info on referring to a package by
name, and [registry](/using-npm/config#registry) for configuring which
registry is used when referring to a package by name.

Examples:
* `semver:@npm:@npmcli/semver-with-patch`
* `semver:@npm:semver@7.2.2`
* `semver:@npm:semver@legacy`

### Folders

* `<folder>`

This refers to a package on the local filesystem.  Specifically this is
a folder with a `package.json` file in it.  This *should* always be
prefixed with a `/` or `./` (or your OS equivalent) to reduce confusion.
npm currently will parse a string with more than one `/` in it as a
folder, but this is legacy behavior that may be removed in a future
version.

Examples:

* `./my-package`
* `/opt/npm/my-package`

### Tarballs

* `<tarball file>`
* `<tarball url>`

Examples:

* `./my-package.tgz`
* `https://registry.npmjs.org/semver/-/semver-1.0.0.tgz`

Refers to a package in a tarball format, either on the local filesystem
or remotely via url.  This is the format that packages exist in when
uploaded to a registry.

### git urls

* `<git:// url>`
* `<github username>/<github project>`

Refers to a package in a git repo.  This can be a full git url, git
shorthand, or a username/package on GitHub.  You can specify a
git tag, branch, or other git ref by appending `#ref`.

Examples:

* `https://github.com/npm/cli.git`
* `git@github.com:npm/cli.git`
* `git+ssh://git@github.com/npm/cli#v6.0.0`
* `github:npm/cli#HEAD`
* `npm/cli#c12ea07`

### See also

[npm-package-arg](https://npm.im/npm-package-arg)
[scope](/using-npm/scope)
[config](/using-npm/config)
