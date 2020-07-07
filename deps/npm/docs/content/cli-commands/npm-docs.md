---
section: cli-commands 
title: npm-docs
description: Docs for a package in a web browser maybe
---

# npm-docs(1) 

## Docs for a package in a web browser maybe

### Synopsis

```bash
npm docs [<pkgname> [<pkgname> ...]]
npm docs .
npm home [<pkgname> [<pkgname> ...]]
npm home .
```

### Description

This command tries to guess at the likely location of a package's
documentation URL, and then tries to open it using the `--browser`
config param. You can pass multiple package names at once. If no
package name is provided, it will search for a `package.json` in
the current folder and use the `name` property.

### Configuration

#### browser

* Default: OS X: `"open"`, Windows: `"start"`, Others: `"xdg-open"`
* Type: String

The browser that is called by the `npm docs` command to open websites.

#### registry

* Default: https://registry.npmjs.org/
* Type: url

The base URL of the npm package registry.


### See Also

* [npm view](/cli-commands/npm-view)
* [npm publish](/cli-commands/npm-publish)
* [npm registry](/using-npm/registry)
* [npm config](/cli-commands/npm-config)
* [npmrc](/configuring-npm/npmrc)
* [package.json](/configuring-npm/package-json)
