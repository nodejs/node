npm-deprecate(1) -- Deprecate a version of a package
====================================================

## SYNOPSIS

    npm deprecate <name>[@<version>] <message>

## DESCRIPTION

This command will update the npm registry entry for a package, providing
a deprecation warning to all who attempt to install it.

It works on version ranges as well as specific versions, so you can do
something like this:

    npm deprecate my-thing@"< 0.2.3" "critical bug fixed in v0.2.3"

Note that you must be the package owner to deprecate something.  See the
`owner` and `adduser` help topics.

## SEE ALSO

* npm-publish(1)
* npm-registry(1)
