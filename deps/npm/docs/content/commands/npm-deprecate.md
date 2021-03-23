---
title: npm-deprecate
section: 1
description: Deprecate a version of a package
---

### Synopsis

```bash
npm deprecate <pkg>[@<version range>] <message>
```

Note: This command is unaware of workspaces.

### Description

This command will update the npm registry entry for a package, providing a
deprecation warning to all who attempt to install it.

It works on [version ranges](https://semver.npmjs.com/) as well as specific
versions, so you can do something like this:

```bash
npm deprecate my-thing@"< 0.2.3" "critical bug fixed in v0.2.3"
```

SemVer ranges passed to this command are interpreted such that they *do*
include prerelease versions.  For example:

```bash
npm deprecate my-thing@1.x "1.x is no longer supported"
```

In this case, a version `my-thing@1.0.0-beta.0` will also be deprecated.

You must be the package owner to deprecate something.  See the `owner` and
`adduser` help topics.

To un-deprecate a package, specify an empty string (`""`) for the `message` 
argument. Note that you must use double quotes with no space between them to 
format an empty string.

### See Also

* [npm publish](/commands/npm-publish)
* [npm registry](/using-npm/registry)
* [npm owner](/cli-commands/owner)
* [npm owner](/cli-commands/adduser)
