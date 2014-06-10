npm-run-script(1) -- Run arbitrary package scripts
==================================================

## SYNOPSIS

    npm run-script [<pkg>] [command]
    npm run [<pkg>] [command]

## DESCRIPTION

This runs an arbitrary command from a package's `"scripts"` object.
If no package name is provided, it will search for a `package.json`
in the current folder and use its `"scripts"` object. If no `"command"`
is provided, it will list the available top level scripts.

It is used by the test, start, restart, and stop commands, but can be
called directly, as well.

## SEE ALSO

* npm-scripts(7)
* npm-test(1)
* npm-start(1)
* npm-restart(1)
* npm-stop(1)
