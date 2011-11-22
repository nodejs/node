npm-restart(3) -- Start a package
=================================

## SYNOPSIS

    npm.commands.restart(packages, callback)

## DESCRIPTION

This runs a package's "restart" script, if one was provided.
Otherwise it runs package's "stop" script, if one was provided, and then
the "start" script.

If no version is specified, then it restarts the "active" version.

npm can run tests on multiple packages. Just specify multiple packages
in the `packages` parameter.

## SEE ALSO

* npm-start(3)
* npm-stop(3)
