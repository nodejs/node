npm-access(1) -- Set access level on published packages
=======================================================

## SYNOPSIS

    npm access public [<package>]
    npm access restricted [<package>]

    npm access add <read-only|read-write> <entity> [<package>]
    npm access rm <entity> [<package>]

    npm access ls [<package>]
    npm access edit [<package>]

## DESCRIPTION

Used to set access controls on private packages.

For all of the subcommands, `npm access` will perform actions on the packages
in the current working directory if no package name is passed to the
subcommand.

* public / restricted:
  Set a package to be either publicly accessible or restricted.

* add / rm:
  Add or remove the ability of users and teams to have read-only or read-write
  access to a package.

* ls:
  Show all of the access privileges for a package. Will only show permissions
  for packages to which you have at least read access.

* edit:
  Set the access privileges for a package at once using `$EDITOR`.

## DETAILS

`npm access` always operates directly on the current registry, configurable
from the command line using `--registry=<registry url>`.

Unscoped packages are *always public*.

Scoped packages *default to restricted*, but you can either publish them as
public using `npm publish --access=public`, or set their access as public using
`npm access public` after the initial publish.

You must have privileges to set the access of a package:

* You are an owner of an unscoped or scoped package.
* You are a member of the team that owns a scope.
* You have been given read-write privileges for a package, either as a member
  of a team or directly as an owner.

If your account is not paid, then attempts to publish scoped packages will fail
with an HTTP 402 status code (logically enough), unless you use
`--access=public`.

## SEE ALSO

* npm-publish(1)
* npm-config(7)
* npm-registry(7)
