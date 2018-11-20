npm-explore(3) -- Browse an installed package
=============================================

## SYNOPSIS

    npm.commands.explore(args, callback)

## DESCRIPTION

Spawn a subshell in the directory of the installed package specified.

If a command is specified, then it is run in the subshell, which then
immediately terminates.

Note that the package is *not* automatically rebuilt afterwards, so be
sure to use `npm rebuild <pkg>` if you make any changes.

The first element in the 'args' parameter must be a package name.  After that is the optional command, which can be any number of strings. All of the strings will be combined into one, space-delimited command.
