npm-restart(1) -- Restart a package
===================================

## SYNOPSIS

    npm restart [-- <args>]

## DESCRIPTION

This restarts a package.

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

## NOTE

Note that the "restart" script is run **in addition to** the "stop"
and "start" scripts, not instead of them.

This is the behavior as of `npm` major version 2.  A change in this
behavior will be accompanied by an increase in major version number

## SEE ALSO

* npm-run-script(1)
* npm-scripts(7)
* npm-test(1)
* npm-start(1)
* npm-stop(1)
* npm-restart(3)