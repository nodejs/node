npm-rebuild(3) -- Rebuild a package
===================================

## SYNOPSIS

    npm.commands.rebuild([packages,] callback)

## DESCRIPTION

This command runs the `npm build` command on each of the matched packages.  This is useful
when you install a new version of node, and must recompile all your C++ addons with
the new binary. If no 'packages' parameter is specify, every package will be rebuilt.

## CONFIGURATION

See `npm help build`
