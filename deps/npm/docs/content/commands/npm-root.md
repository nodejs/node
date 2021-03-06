---
title: npm-root
section: 1
description: Display npm root
---

### Synopsis

```bash
npm root [-g]
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

### See Also

* [npm prefix](/commands/npm-prefix)
* [npm bin](/commands/npm-bin)
* [npm folders](/configuring-npm/folders)
* [npm config](/commands/npm-config)
* [npmrc](/configuring-npm/npmrc)
