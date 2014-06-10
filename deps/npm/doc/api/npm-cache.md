npm-cache(3) -- manage the npm cache programmatically
=====================================================

## SYNOPSIS

    npm.commands.cache([args], callback)

    // helpers
    npm.commands.cache.clean([args], callback)
    npm.commands.cache.add([args], callback)
    npm.commands.cache.read(name, version, forceBypass, callback)

## DESCRIPTION

This acts much the same ways as the npm-cache(1) command line
functionality.

The callback is called with the package.json data of the thing that is
eventually added to or read from the cache.

The top level `npm.commands.cache(...)` functionality is a public
interface, and like all commands on the `npm.commands` object, it will
match the command line behavior exactly.

However, the cache folder structure and the cache helper functions are
considered **internal** API surface, and as such, may change in future
releases of npm, potentially without warning or significant version
incrementation.

Use at your own risk.
