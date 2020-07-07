---
section: cli-commands 
title: npm-edit
description: Edit an installed package
---

# npm-edit(1)

## Edit an installed package

### Synopsis

```bash
npm edit <pkg>[/<subpkg>...]
```

### Description

Selects a (sub)dependency in the current
working directory and opens the package folder in the default editor
(or whatever you've configured as the npm `editor` config -- see
[`npm-config`](npm-config).)

After it has been edited, the package is rebuilt so as to pick up any
changes in compiled packages.

For instance, you can do `npm install connect` to install connect
into your package, and then `npm edit connect` to make a few
changes to your locally installed copy.

### Configuration

#### editor

* Default: `EDITOR` environment variable if set, or `"vi"` on Posix,
  or `"notepad"` on Windows.
* Type: path

The command to run for `npm edit` or `npm config edit`.

### See Also

* [npm folders](/configuring-npm/folders)
* [npm explore](/cli-commands/npm-explore)
* [npm install](/cli-commands/npm-install)
* [npm config](/cli-commands/npm-config)
* [npmrc](/configuring-npm/npmrc)
