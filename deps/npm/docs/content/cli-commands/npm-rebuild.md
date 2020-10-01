---
section: cli-commands 
title: npm-rebuild
description: Rebuild a package
---

# npm-rebuild(1)

## Rebuild a package

### Synopsis

```bash
npm rebuild [[<@scope>/<name>]...]

alias: npm rb
```

### Description

This command runs the `npm build` command on the matched folders.  This is useful when you install a new version of node, and must recompile all your C++ addons with the new binary.

### See Also

* [npm build](/cli-commands/build)
* [npm install](/cli-commands/install)
