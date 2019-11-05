---
section: cli-commands 
title: npm-repo
description: Open package repository page in the browser
---

# npm-repo

## Open package repository page in the browser

### Synopsis

```bash
npm repo [<pkg>]
```

### Description

This command tries to guess at the likely location of a package's
repository URL, and then tries to open it using the `--browser`
config param. If no package name is provided, it will search for
a `package.json` in the current folder and use the `name` property.

### Configuration

#### browser

* Default: OS X: `"open"`, Windows: `"start"`, Others: `"xdg-open"`
* Type: String

The browser that is called by the `npm repo` command to open websites.

### See Also

* [npm docs](/cli-commands/npm-docs)
* [npm config](/cli-commands/npm-config)
