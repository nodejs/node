---
title: npm-root
section: 1
description: Display npm root
---

### Synopsis

```bash
npm root
```

### Description

Print the effective `node_modules` folder to standard out.

Useful for using npm in shell scripts that do things with the
`node_modules` folder.  For example:

```bash
#!/bin/bash
global_node_modules="$(npm root --global)"
echo "Global packages installed in: ${global_node_modules}"
```

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
* [npm bin](/commands/npm-bin)
* [npm folders](/configuring-npm/folders)
* [npm config](/commands/npm-config)
* [npmrc](/configuring-npm/npmrc)
