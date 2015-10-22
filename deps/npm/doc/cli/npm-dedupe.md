npm-dedupe(1) -- Reduce duplication
===================================

## SYNOPSIS

    npm dedupe [package names...]
    npm ddp [package names...]

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

If a suitable version exists at the target location in the tree
already, then it will be left untouched, but the other duplicates will
be deleted.

If no suitable version can be found, then a warning is printed, and
nothing is done.

If any arguments are supplied, then they are filters, and only the
named packages will be touched.

Note that this operation transforms the dependency tree, and may
result in packages getting updated versions, perhaps from the npm
registry.

This feature is experimental, and may change in future versions.

The `--tag` argument will apply to all of the affected dependencies. If a
tag with the given name exists, the tagged version is preferred over newer
versions.

## SEE ALSO

* npm-ls(1)
* npm-update(1)
* npm-install(1)
