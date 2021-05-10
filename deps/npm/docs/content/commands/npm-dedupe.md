---
title: npm-dedupe
section: 1
description: Reduce duplication in the package tree
---

### Synopsis

```bash
npm dedupe
npm ddp

aliases: ddp
```

### Description

Searches the local package tree and attempts to simplify the overall
structure by moving dependencies further up the tree, where they can
be more effectively shared by multiple dependent packages.

For example, consider this dependency graph:

```
a
+-- b <-- depends on c@1.0.x
|   `-- c@1.0.3
`-- d <-- depends on c@~1.0.9
    `-- c@1.0.10
```

In this case, `npm dedupe` will transform the tree to:

```bash
a
+-- b
+-- d
`-- c@1.0.10
```

Because of the hierarchical nature of node's module lookup, b and d
will both get their dependency met by the single c package at the root
level of the tree.

In some cases, you may have a dependency graph like this:

```
a
+-- b <-- depends on c@1.0.x
+-- c@1.0.3
`-- d <-- depends on c@1.x
    `-- c@1.9.9
```

During the installation process, the `c@1.0.3` dependency for `b` was
placed in the root of the tree.  Though `d`'s dependency on `c@1.x` could
have been satisfied by `c@1.0.3`, the newer `c@1.9.0` dependency was used,
because npm favors updates by default, even when doing so causes
duplication.

Running `npm dedupe` will cause npm to note the duplication and
re-evaluate, deleting the nested `c` module, because the one in the root is
sufficient.

To prefer deduplication over novelty during the installation process, run
`npm install --prefer-dedupe` or `npm config set prefer-dedupe true`.

Arguments are ignored. Dedupe always acts on the entire tree.

Note that this operation transforms the dependency tree, but will never
result in new modules being installed.

Using `npm find-dupes` will run the command in `--dry-run` mode.

### See Also

* [npm find-dupes](/commands/npm-find-dupes)
* [npm ls](/commands/npm-ls)
* [npm update](/commands/npm-update)
* [npm install](/commands/npm-install)
