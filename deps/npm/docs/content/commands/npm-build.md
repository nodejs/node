---
title: npm-build
section: 1
description: Build a package
---

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
    npm build
```

### See Also

* [npm install](/commands/npm-install)
* [npm link](/commands/npm-link)
* [npm scripts](/using-npm/scripts)
* [package.json](/configuring-npm/package-json)
