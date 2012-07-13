npm-version(1) -- Bump a package version
========================================

## SYNOPSIS

    npm version [<newversion> | major | minor | patch | build]

## DESCRIPTION

Run this in a package directory to bump the version and write the new
data back to the package.json file.

The `newversion` argument should be a valid semver string, *or* a valid
second argument to semver.inc (one of "build", "patch", "minor", or
"major"). In the second case, the existing version will be incremented
by 1 in the specified field.

If run in a git repo, it will also create a version commit and tag, and
fail if the repo is not clean.

If supplied with `--message` (shorthand: `-m`) config option, npm will
use it as a commit message when creating a version commit.  If the
`message` config contains `%s` then that will be replaced with the
resulting version number.  For example:

    npm version patch -m "Upgrade to %s for reasons"

If the `sign-git-tag` config is set, then the tag will be signed using
the `-s` flag to git.  Note that you must have a default GPG key set up
in your git config for this to work properly.

## SEE ALSO

* npm-init(1)
* npm-json(1)
* npm-semver(1)
