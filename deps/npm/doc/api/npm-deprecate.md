npm-deprecate(3) -- Deprecate a version of a package
====================================================

## SYNOPSIS

    npm.commands.deprecate(args, callback)

## DESCRIPTION

This command will update the npm registry entry for a package, providing
a deprecation warning to all who attempt to install it.

The 'args' parameter must have exactly two elements:

* `package[@version]`

    The `version` portion is optional, and may be either a range, or a
    specific version, or a tag.

* `message`

    The warning message that will be printed whenever a user attempts to
    install the package.

Note that you must be the package owner to deprecate something.  See the
`owner` and `adduser` help topics.

To un-deprecate a package, specify an empty string (`""`) for the `message` argument.

## SEE ALSO

* npm-publish(3)
* npm-unpublish(3)
* npm-registry(7)
