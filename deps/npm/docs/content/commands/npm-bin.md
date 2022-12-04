---
title: npm-bin
section: 1
description: Display npm bin folder
---

### Synopsis

```bash
npm bin
```

Note: This command is unaware of workspaces.

### Description

Print the folder where npm will install executables.

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

### See Also

* [npm prefix](/commands/npm-prefix)
* [npm root](/commands/npm-root)
* [npm folders](/configuring-npm/folders)
* [npm config](/commands/npm-config)
* [npmrc](/configuring-npm/npmrc)
