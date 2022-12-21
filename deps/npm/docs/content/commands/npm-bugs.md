---
title: npm-bugs
section: 1
description: Bugs for a package in a web browser maybe
---

### Synopsis
```bash
npm bugs [<pkgname>]

aliases: issues
```

### Description

This command tries to guess at the likely location of a package's
bug tracker URL, and then tries to open it using the `--browser`
config param. If no package name is provided, it will search for
a `package.json` in the current folder and use the `name` property.

### Configuration

#### browser

* Default: OS X: `"open"`, Windows: `"start"`, Others: `"xdg-open"`
* Type: String

The browser that is called by the `npm bugs` command to open websites.

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
