npm-test(3) -- Test a package
=============================

## SYNOPSIS

      npm.commands.test(packages, callback)

## DESCRIPTION

This runs a package's "test" script, if one was provided.

To run tests as a condition of installation, set the `npat` config to
true.

npm can run tests on multiple packages. Just specify multiple packages
in the `packages` parameter.
