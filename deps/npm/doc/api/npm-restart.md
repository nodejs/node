npm-restart(3) -- Restart a package
===================================

## SYNOPSIS

    npm.commands.restart(packages, callback)

## DESCRIPTION

This restarts a package (or multiple packages).

This runs a package's "stop", "restart", and "start" scripts, and associated
pre- and post- scripts, in the order given below:

1. prerestart
2. prestop
3. stop
4. poststop
5. restart
6. prestart
7. start
8. poststart
9. postrestart

If no version is specified, then it restarts the "active" version.

npm can restart multiple packages. Just specify multiple packages in
the `packages` parameter.

## NOTE

Note that the "restart" script is run **in addition to** the "stop"
and "start" scripts, not instead of them.

This is the behavior as of `npm` major version 2.  A change in this
behavior will be accompanied by an increase in major version number

## SEE ALSO

* npm-start(3)
* npm-stop(3)
