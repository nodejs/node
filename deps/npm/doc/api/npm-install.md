npm-install(3) -- install a package programmatically
====================================================

## SYNOPSIS

    npm.commands.install([where,] packages, callback)

## DESCRIPTION

This acts much the same ways as installing on the command-line.

The 'where' parameter is optional and only used internally, and it specifies
where the packages should be installed to.

The 'packages' parameter is an array of strings. Each element in the array is
the name of a package to be installed.

Finally, 'callback' is a function that will be called when all packages have been
installed or when an error has been encountered.
