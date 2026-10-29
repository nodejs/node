---
title: npm-set
section: 1
description: Set a value in the npm configuration
---

### Synopsis

```bash
npm set <key>=<value> [<key>=<value> ...] (See `npm config`)
```

Note: This command is unaware of workspaces.

### Description

Set a value in the npm configuration

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



#### `location`

* Default: "user" unless `--global` is passed, which will also set this value
  to "global"
* Type: "global", "user", or "project"

When passed to `npm config` this refers to which config file to use.

When set to "global" mode, packages are installed into the `prefix` folder
instead of the current working directory. See
[folders](/configuring-npm/folders) for more on the differences in behavior.

* packages are installed into the `{prefix}/lib/node_modules` folder, instead
  of the current working directory.
* bin files are linked to `{prefix}/bin`
* man pages are linked to `{prefix}/share/man`



### See Also

* [npm help config](/commands/npm-config)
