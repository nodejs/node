---
title: npm-bugs
section: 1
description: Report bugs for a package in a web browser
---

### Synopsis

```bash
npm bugs [<pkgname> [<pkgname> ...]]

aliases: issues
```

### Description

This command tries to guess at the likely location of a package's bug
tracker URL or the `mailto` URL of the support email, and then tries to
open it using the `--browser` config param. If no package name is provided, it
will search for a `package.json` in the current folder and use the `name` property.

### Configuration

#### browser

* Default: OS X: `"open"`, Windows: `"start"`, Others: `"xdg-open"`
* Type: String or Boolean

The browser that is called by the `npm bugs` command to open websites.

Set to `false` to suppress browser behavior and instead print urls to
terminal.

Set to `true` to use default system URL opener.

#### registry

* Default: https://registry.npmjs.org/
* Type: url

The base URL of the npm package registry.

### See Also

* [npm docs](/commands/npm-docs)
* [npm view](/commands/npm-view)
* [npm publish](/commands/npm-publish)
* [npm registry](/using-npm/registry)
* [npm config](/commands/npm-config)
* [npmrc](/configuring-npm/npmrc)
* [package.json](/configuring-npm/package-json)
