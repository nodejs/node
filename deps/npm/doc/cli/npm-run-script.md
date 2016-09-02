npm-run-script(1) -- Run arbitrary package scripts
==================================================

## SYNOPSIS

    npm run-script <command> [-- <args>...]

    alias: npm run

## DESCRIPTION

This runs an arbitrary command from a package's `"scripts"` object.  If no
`"command"` is provided, it will list the available scripts.  `run[-script]` is
used by the test, start, restart, and stop commands, but can be called
directly, as well. When the scripts in the package are printed out, they're
separated into lifecycle (test, start, restart) and directly-run scripts.

As of [`npm@2.0.0`](http://blog.npmjs.org/post/98131109725/npm-2-0-0), you can
use custom arguments when executing scripts. The special option `--` is used by
[getopt](http://goo.gl/KxMmtG) to delimit the end of the options. npm will pass
all the arguments after the `--` directly to your script:

    npm run test -- --grep="pattern"

The arguments will only be passed to the script specified after ```npm run```
and not to any pre or post script.

The `env` script is a special built-in command that can be used to list
environment variables that will be available to the script at runtime. If an
"env" command is defined in your package it will take precedence over the
built-in.

In addition to the shell's pre-existing `PATH`, `npm run` adds
`node_modules/.bin` to the `PATH` provided to scripts. Any binaries provided by
locally-installed dependencies can be used without the `node_modules/.bin`
prefix. For example, if there is a `devDependency` on `tap` in your package,
you should write:

    "scripts": {"test": "tap test/\*.js"}

instead of `"scripts": {"test": "node_modules/.bin/tap test/\*.js"}` to run your tests.

`npm run` sets the `NODE` environment variable to the `node` executable with
which `npm` is executed. Also, the directory within which it resides is added to the
`PATH`, if the `node` executable is not in the `PATH`.

If you try to run a script without having a `node_modules` directory and it fails,
you will be given a warning to run `npm install`, just in case you've forgotten.

## SEE ALSO

* npm-scripts(7)
* npm-test(1)
* npm-start(1)
* npm-restart(1)
* npm-stop(1)
