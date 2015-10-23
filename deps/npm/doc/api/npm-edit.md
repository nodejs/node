npm-edit(3) -- Edit an installed package
========================================

## SYNOPSIS

    npm.commands.edit(package, callback)

## DESCRIPTION

Opens the package folder in the default editor (or whatever you've
configured as the npm `editor` config -- see `npm help config`.)

After it has been edited, the package is rebuilt so as to pick up any
changes in compiled packages.

For instance, you can do `npm install connect` to install connect
into your package, and then `npm.commands.edit(["connect"], callback)`
to make a few changes to your locally installed copy.

The first parameter is a string array with a single element, the package
to open. The package can optionally have a version number attached.

Since this command opens an editor in a new process, be careful about where
and how this is used.
