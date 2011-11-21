npm-version(1) -- Bump a package version
========================================

## SYNOPSIS

    npm version <newversion> [--message commit-message]

## DESCRIPTION

Run this in a package directory to bump the version and write the new
data back to the package.json file.

The `newversion` argument should be a valid semver string, *or* a valid
second argument to semver.inc (one of "patch", "minor", or "major"). In 
the second case, the existing version will be incremented by that amount.

If run in a git repo, it will also create a version commit and tag, and
fail if the repo is not clean.

If supplied with `--message` (shorthand: `-m`) command line option, npm
will use it as a commit message when creating a version commit.

## SEE ALSO

* npm-init(1)
* npm-json(1)
* npm-semver(1)
