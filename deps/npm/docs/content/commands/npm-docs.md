---
title: npm-docs
section: 1
description: Open documentation for a package in a web browser
---

### Synopsis

```bash
npm docs [<pkgname> [<pkgname> ...]]

aliases: home
```

### Description

This command tries to guess at the likely location of a package's
documentation URL, and then tries to open it using the `--browser` config
param. You can pass multiple package names at once. If no package name is
provided, it will search for a `package.json` in the current folder and use
the `name` property.

### Configuration

#### browser

* Default: OS X: `"open"`, Windows: `"start"`, Others: `"xdg-open"`
* Type: String or Boolean

The browser that is called by the `npm docs` command to open websites.

Set to `false` to suppress browser behavior and instead print urls to
terminal.

Set to `true` to use default system URL opener.

#### registry

* Default: https://registry.npmjs.org/
* Type: url

The base URL of the npm package registry.

### See Also

* [npm view](/commands/npm-view)
* [npm publish](/commands/npm-publish)
* [npm registry](/using-npm/registry)
* [npm config](/commands/npm-config)
* [npmrc](/configuring-npm/npmrc)
* [package.json](/configuring-npm/package-json)
