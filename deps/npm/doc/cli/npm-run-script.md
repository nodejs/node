npm-run-script(1) -- Run arbitrary package scripts
==================================================

## SYNOPSIS

    npm run-script [command] [-- <args>]
    npm run [command] [-- <args>]

## DESCRIPTION

This runs an arbitrary command from a package's `"scripts"` object.
If no package name is provided, it will search for a `package.json`
in the current folder and use its `"scripts"` object. If no `"command"`
is provided, it will list the available top level scripts.

It is used by the test, start, restart, and stop commands, but can be
called directly, as well.

As of [`npm@2.0.0`](http://blog.npmjs.org/post/98131109725/npm-2-0-0), you can
use custom arguments when executing scripts. The special option `--` is used by
[getopt](http://goo.gl/KxMmtG) to delimit the end of the options. npm will pass
all the arguments after the `--` directly to your script:

    npm run test -- --grep="pattern"

The arguments will only be passed to the script specified after ```npm run```
and not to any pre or post script.

## SEE ALSO

* npm-scripts(7)
* npm-test(1)
* npm-start(1)
* npm-restart(1)
* npm-stop(1)
