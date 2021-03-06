---
title: npm-repo
section: 1
description: Open package repository page in the browser
---

### Synopsis

```bash
npm repo [<pkgname> [<pkgname> ...]]
```

### Description

This command tries to guess at the likely location of a package's
repository URL, and then tries to open it using the `--browser` config
param. If no package name is provided, it will search for a `package.json`
in the current folder and use the `repository` property.

### Configuration

#### browser

* Default: OS X: `"open"`, Windows: `"start"`, Others: `"xdg-open"`
* Type: String or Boolean

The browser that is called by the `npm repo` command to open websites.

Set to `false` to suppress browser behavior and instead print urls to
terminal.

Set to `true` to use default system URL opener.

### See Also

* [npm docs](/commands/npm-docs)
* [npm config](/commands/npm-config)
