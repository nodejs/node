npm-build(1) -- Build a package
===============================

## SYNOPSIS

    npm build [<package-folder>]

* `<package-folder>`:
  A folder containing a `package.json` file in its root.

## DESCRIPTION

This is the plumbing command called by `npm link` and `npm install`.

It should generally be called during installation, but if you need to run it
directly, run:

    npm run-script build

## SEE ALSO

* npm-install(1)
* npm-link(1)
* npm-scripts(7)
* package.json(5)
