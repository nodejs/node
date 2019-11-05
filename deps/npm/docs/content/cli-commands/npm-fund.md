---
section: cli-commands 
title: npm-fund
description: Retrieve funding information
---

# npm-fund

## Retrieve funding information

### Synopsis

```bash
    npm fund [<pkg>]
```

### Description

This command retrieves information on how to fund the dependencies of
a given project. If no package name is provided, it will list all
dependencies that are looking for funding in a tree-structure in which
are listed the type of funding and the url to visit. If a package name
is provided then it tries to open its funding url using the `--browser`
config param.

The list will avoid duplicated entries and will stack all packages
that share the same type/url as a single entry. Given this nature the
list is not going to have the same shape of the output from `npm ls`.

### Configuration

#### browser

* Default: OS X: `"open"`, Windows: `"start"`, Others: `"xdg-open"`
* Type: String

The browser that is called by the `npm fund` command to open websites.

#### json

* Default: false
* Type: Boolean

Show information in JSON format.

#### unicode

* Type: Boolean
* Default: true

Whether to represent the tree structure using unicode characters.
Set it to `false` in order to use all-ansi output.

## See Also

* [npm-docs](/cli-commands/npm-docs)
* [npm-config](/cli-commands/npm-config)
* [npm-install](/cli-commands/npm-install)
* [npm-ls](/cli-commands/npm-ls)

