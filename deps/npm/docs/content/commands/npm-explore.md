---
title: npm-explore
section: 1
description: Browse an installed package
---

### Synopsis

```bash
npm explore <pkg> [ -- <command>]
```

Note: This command is unaware of workspaces.

### Description

Spawn a subshell in the directory of the installed package specified.

If a command is specified, then it is run in the subshell, which then
immediately terminates.

This is particularly handy in the case of git submodules in the
`node_modules` folder:

```bash
npm explore some-dependency -- git pull origin master
```

Note that the package is *not* automatically rebuilt afterwards, so be
sure to use `npm rebuild <pkg>` if you make any changes.

### Configuration

#### shell

* Default: SHELL environment variable, or "bash" on Posix, or "cmd" on
  Windows
* Type: path

The shell to run for the `npm explore` command.

### See Also

* [npm folders](/configuring-npm/folders)
* [npm edit](/commands/npm-edit)
* [npm rebuild](/commands/npm-rebuild)
* [npm install](/commands/npm-install)
