npm-ls(3) -- List installed packages
======================================

## SYNOPSIS

    npm.commands.ls(args, [silent,] callback)

## DESCRIPTION

This command will print to stdout all the versions of packages that are
installed, as well as their dependencies, in a tree-structure. It will also
return that data using the callback.

This command does not take any arguments, but args must be defined.
Beyond that, if any arguments are passed in, npm will politely warn that it
does not take positional arguments, though you may set config flags
like with any other command, such as `global` to list global packages.

It will print out extraneous, missing, and invalid packages.

If the silent parameter is set to true, nothing will be output to the screen,
but the data will still be returned.

Callback is provided an error if one occurred, the full data about which
packages are installed and which dependencies they will receive, and a
"lite" data object which just shows which versions are installed where.
Note that the full data object is a circular structure, so care must be
taken if it is serialized to JSON.

## CONFIGURATION

### long

* Default: false
* Type: Boolean

Show extended information.

### parseable

* Default: false
* Type: Boolean

Show parseable output instead of tree view.

### global

* Default: false
* Type: Boolean

List packages in the global install prefix instead of in the current
project.

Note, if parseable is set or long isn't set, then duplicates will be trimmed.
This means that if a submodule a same dependency as a parent module, then the
dependency will only be output once.
