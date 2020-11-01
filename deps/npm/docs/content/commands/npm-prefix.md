---
title: npm-prefix
section: 1
description: Display prefix
---

### Synopsis

```bash
npm prefix [-g]
```

### Description

Print the local prefix to standard out. This is the closest parent directory
to contain a `package.json` file or `node_modules` directory, unless `-g` is
also specified.

If `-g` is specified, this will be the value of the global prefix. See
[`npm config`](/commands/npm-config) for more detail.

### See Also

* [npm root](/commands/npm-root)
* [npm bin](/commands/npm-bin)
* [npm folders](/configuring-npm/folders)
* [npm config](/commands/npm-config)
* [npmrc](/configuring-npm/npmrc)
