npm-repo(3) -- Open package repository page in the browser
========================================================

## SYNOPSIS

    npm.commands.repo(package, callback)

## DESCRIPTION

This command tries to guess at the likely location of a package's
repository URL, and then tries to open it using the `--browser`
config param.

Like other commands, the first parameter is an array. This command only
uses the first element, which is expected to be a package name with an
optional version number.

This command will launch a browser, so this command may not be the most
friendly for programmatic use.
