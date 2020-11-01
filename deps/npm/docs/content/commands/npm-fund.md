---
title: npm-fund
section: 1
description: Retrieve funding information
---

### Synopsis

```bash
npm fund [<pkg>]
```

### Description

This command retrieves information on how to fund the dependencies of a
given project. If no package name is provided, it will list all
dependencies that are looking for funding in a tree structure, listing the
type of funding and the url to visit. If a package name is provided then it
tries to open its funding url using the `--browser` config param; if there
are multiple funding sources for the package, the user will be instructed
to pass the `--which` option to disambiguate.

The list will avoid duplicated entries and will stack all packages that
share the same url as a single entry. Thus, the list does not have the same
shape of the output from `npm ls`.

### Configuration

#### browser

* Default: OS X: `"open"`, Windows: `"start"`, Others: `"xdg-open"`
* Type: String

The browser that is called by the `npm fund` command to open websites.

#### json

* Type: Boolean
* Default: false

Show information in JSON format.

#### unicode

* Type: Boolean
* Default: true

Whether to represent the tree structure using unicode characters.
Set it to `false` in order to use all-ansi output.

#### which

* Type: Number
* Default: undefined

If there are multiple funding sources, which 1-indexed source URL to open.

## See Also

* [npm install](/commands/npm-install)
* [npm docs](/commands/npm-docs)
* [npm ls](/commands/npm-ls)
* [npm config](/commands/npm-config)
