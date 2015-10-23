npm-rebuild(1) -- Rebuild a package
===================================

## SYNOPSIS

    npm rebuild [<name> [<name> ...]]
    npm rb [<name> [<name> ...]]

* `<name>`:
  The package to rebuild

## DESCRIPTION

This command runs the `npm build` command on the matched folders.  This is useful
when you install a new version of node, and must recompile all your C++ addons with
the new binary.

## SEE ALSO

* npm-build(1)
* npm-install(1)
