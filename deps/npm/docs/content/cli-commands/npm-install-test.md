---
section: cli-commands 
title: npm-install-test
description: Install package(s) and run tests
---

# npm install-test(1)

## Install package(s) and run tests

### Synopsis

```bash
npm install-test (with no args, in package dir)
npm install-test [<@scope>/]<name>
npm install-test [<@scope>/]<name>@<tag>
npm install-test [<@scope>/]<name>@<version>
npm install-test [<@scope>/]<name>@<version range>
npm install-test <tarball file>
npm install-test <tarball url>
npm install-test <folder>

alias: npm it
common options: [--save|--save-dev|--save-optional] [--save-exact] [--dry-run]
```

### Description

This command runs an `npm install` followed immediately by an `npm test`. It
takes exactly the same arguments as `npm install`.

### See Also

* [npm install](/cli-commands/install)
* [npm test](/cli-commands/test)
