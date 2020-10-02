---
section: cli-commands 
title: npm-prune
description: Remove extraneous packages
---

# npm-prune(1)
## Remove extraneous packages

### Synopsis

```bash
npm prune [[<@scope>/]<pkg>...] [--production] [--dry-run] [--json]
```

### Description

This command removes "extraneous" packages.  If a package name is
provided, then only packages matching one of the supplied names are
removed.

Extraneous packages are packages that are not listed on the parent
package's dependencies list.

If the `--production` flag is specified or the `NODE_ENV` environment
variable is set to `production`, this command will remove the packages
specified in your `devDependencies`. Setting `--no-production` will
negate `NODE_ENV` being set to `production`.

If the `--dry-run` flag is used then no changes will actually be made.

If the `--json` flag is used then the changes `npm prune` made (or would
have made with `--dry-run`) are printed as a JSON object.

In normal operation with package-locks enabled, extraneous modules are
pruned automatically when modules are installed and you'll only need
this command with the `--production` flag.

If you've disabled package-locks then extraneous modules will not be removed
and it's up to you to run `npm prune` from time-to-time to remove them.

### See Also

* [npm uninstall](/cli-commands/uninstall)
* [npm folders](/configuring-npm/folders)
* [npm ls](/cli-commands/ls)
