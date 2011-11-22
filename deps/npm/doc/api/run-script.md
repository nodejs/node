npm-run-script(3) -- Run arbitrary package scripts
==================================================

## SYNOPSIS

    npm.commands.run-script(args, callback)

## DESCRIPTION

This runs an arbitrary command from a package's "scripts" object.

It is used by the test, start, restart, and stop commands, but can be
called directly, as well.

The 'args' parameter is an array of strings. Behavior depends on the number
of elements.  If there is only one element, npm assumes that the element
represents a command to be run on the local repository. If there is more than
one element, then the first is assumed to be the package and the second is
assumed to be the command to run. All other elements are ignored.

## SEE ALSO

* npm-scripts(1)
* npm-test(3)
* npm-start(3)
* npm-restart(3)
* npm-stop(3)
