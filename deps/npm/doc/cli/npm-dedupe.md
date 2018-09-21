npm-dedupe(1) -- Reduce duplication
===================================

## SYNOPSIS

    npm dedupe
    npm ddp

    aliases: find-dupes, ddp

## DESCRIPTION

Searches the local package tree and attempts to simplify the overall
structure by moving dependencies further up the tree, where they can
be more effectively shared by multiple dependent packages.

For example, consider this dependency graph:

    a
    +-- b <-- depends on c@1.0.x
    |   `-- c@1.0.3
    `-- d <-- depends on c@~1.0.9
        `-- c@1.0.10

In this case, `npm-dedupe(1)` will transform the tree to:

    a
    +-- b
    +-- d
    `-- c@1.0.10

Because of the hierarchical nature of node's module lookup, b and d
will both get their dependency met by the single c package at the root
level of the tree.

The deduplication algorithm walks the tree, moving each dependency as far
up in the tree as possible, even if duplicates are not found. This will
result in both a flat and deduplicated tree.

If a suitable version exists at the target location in the tree
already, then it will be left untouched, but the other duplicates will
be deleted.

Arguments are ignored. Dedupe always acts on the entire tree.

Modules

Note that this operation transforms the dependency tree, but will never
result in new modules being installed.

## SEE ALSO

* npm-ls(1)
* npm-update(1)
* npm-install(1)
