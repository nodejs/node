npm-shrinkwrap(1) -- Lock down dependency versions for publication
=====================================================

## SYNOPSIS

    npm shrinkwrap

## DESCRIPTION

This command repurposes `package-lock.json` into a publishable
`npm-shrinkwrap.json` or simply creates a new one. The file created and updated
by this command will then take precedence over any other existing or future
`package-lock.json` files. For a detailed explanation of the design and purpose
of package locks in npm, see npm-package-locks(5).

## SEE ALSO

* npm-install(1)
* npm-run-script(1)
* npm-scripts(7)
* package.json(5)
* npm-package-locks(5)
* package-lock.json(5)
* npm-shrinkwrap.json(5)
* npm-ls(1)
