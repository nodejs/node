npm-tag(3) -- Tag a published version
=====================================

## SYNOPSIS

    npm.commands.tag(package@version, tag, callback)

## DESCRIPTION

Tags the specified version of the package with the specified tag, or the
`--tag` config if not specified.

The 'package@version' is an array of strings, but only the first two elements are
currently used.

The first element must be in the form package@version, where package
is the package name and version is the version number (much like installing a
specific version).

The second element is the name of the tag to tag this version with. If this
parameter is missing or falsey (empty), the default from the config will be
used. For more information about how to set this config, check
`man 3 npm-config` for programmatic usage or `man npm-config` for cli usage.
