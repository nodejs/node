npm-uninstall(3) -- uninstall a package programmatically
========================================================

## SYNOPSIS

    npm.commands.uninstall(packages, callback)

## DESCRIPTION

This acts much the same ways as uninstalling on the command-line.

The 'packages' parameter is an array of strings. Each element in the array is
the name of a package to be uninstalled.

Finally, 'callback' is a function that will be called when all packages have been
uninstalled or when an error has been encountered.
