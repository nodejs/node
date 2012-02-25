npm-shrinkwrap(3) -- programmatically generate package shrinkwrap file
====================================================

## SYNOPSIS

    npm.commands.shrinkwrap(args, [silent,] callback)

## DESCRIPTION

This acts much the same ways as shrinkwrapping on the command-line.

This command does not take any arguments, but 'args' must be defined.
Beyond that, if any arguments are passed in, npm will politely warn that it
does not take positional arguments.

If the 'silent' parameter is set to true, nothing will be output to the screen,
but the shrinkwrap file will still be written.

Finally, 'callback' is a function that will be called when the shrinkwrap has
been saved.
