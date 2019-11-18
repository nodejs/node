---
section: cli-commands 
title: npm-build
description: Build a package
---

# npm-build(1)

## Build a package

### Synopsis
```shell
npm build [<package-folder>]
```

* `<package-folder>`:
  A folder containing a `package.json` file in its root.

### Description

This is the plumbing command called by `npm link` and `npm install`.

It should generally be called during installation, but if you need to run it
directly, run:
```bash
    npm run-script build
```

### See Also

* [npm install](/cli-commands/npm-install)
* [npm link](/cli-commands/npm-link)
* [npm scripts](/using-npm/scripts)
* [package.json](/configuring-npm/package-json)
