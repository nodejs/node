npm-version(3) -- Bump a package version
========================================

## SYNOPSIS

    npm.commands.version(newversion, callback)

## DESCRIPTION

Run this in a package directory to bump the version and write the new
data back to the package.json file.

If run in a git repo, it will also create a version commit and tag, and
fail if the repo is not clean.

Like all other commands, this function takes a string array as its first
parameter. The difference, however, is this function will fail if it does
not have exactly one element. The only element should be a version number.
