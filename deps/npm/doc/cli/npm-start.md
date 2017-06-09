npm-start(1) -- Start a package
===============================

## SYNOPSIS

    npm start [-- <args>]

## DESCRIPTION

This runs an arbitrary command specified in the package's `"start"` property of
its `"scripts"` object. If no `"start"` property is specified on the
`"scripts"` object, it will run `node server.js`.

As of [`npm@2.0.0`](http://blog.npmjs.org/post/98131109725/npm-2-0-0), you can
use custom arguments when executing scripts. Refer to npm-run-script(1) for
more details.

## SEE ALSO

* npm-run-script(1)
* npm-scripts(7)
* npm-test(1)
* npm-restart(1)
* npm-stop(1)
