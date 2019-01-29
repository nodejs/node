npm-prefix(1) -- Display prefix
===============================

## SYNOPSIS

    npm prefix [-g]

## DESCRIPTION

Print the local prefix to standard out. This is the closest parent directory
to contain a `package.json` file or `node_modules` directory, unless `-g` is
also specified.

If `-g` is specified, this will be the value of the global prefix. See
`npm-config(7)` for more detail.

## SEE ALSO

* npm-root(1)
* npm-bin(1)
* npm-folders(5)
* npm-config(1)
* npm-config(7)
* npmrc(5)
