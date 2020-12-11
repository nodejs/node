---
title: npm-rebuild
section: 1
description: Rebuild a package
---

### Synopsis

```bash
npm rebuild [[<@scope>/]<name>[@<version>] ...]

alias: rb
```

### Description

This command runs the `npm build` command on the matched folders.  This is
useful when you install a new version of node, and must recompile all your
C++ addons with the new binary.  It is also useful when installing with
`--ignore-scripts` and `--no-bin-links`, to explicitly choose which
packages to build and/or link bins.

If one or more package names (and optionally version ranges) are provided,
then only packages with a name and version matching one of the specifiers
will be rebuilt.

### See Also

* [npm install](/commands/npm-install)
