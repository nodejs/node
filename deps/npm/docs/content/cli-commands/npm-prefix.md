---
section: cli-commands 
title: npm-prefix
description: Display prefix
---

# npm-prefix(1)

## Display prefix

### Synopsis

```bash
npm prefix [-g]
```

### Description

Print the local prefix to standard out. This is the closest parent directory
to contain a `package.json` file or `node_modules` directory, unless `-g` is
also specified.

If `-g` is specified, this will be the value of the global prefix. See
[`npm config`](/cli-commands/config) for more detail.

### See Also

* [npm root](/cli-commands/root)
* [npm bin](/cli-commands/bin)
* [npm folders](/configuring-npm/folders)
* [npm config](/cli-commands/config)
* [npmrc](/configuring-npm/npmrc)
