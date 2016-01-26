npm-logout(1) -- Log out of the registry
========================================

## SYNOPSIS

    npm logout [--registry=url] [--scope=@orgname]

## DESCRIPTION

When logged into a registry that supports token-based authentication, tell the
server to end this token's session. This will invalidate the token everywhere
you're using it, not just for the current environment.

When logged into a legacy registry that uses username and password authentication, this will
clear the credentials in your user configuration. In this case, it will _only_ affect
the current environment.

If `--scope` is provided, this will find the credentials for the registry
connected to that scope, if set.

## CONFIGURATION

### registry

Default: https://registry.npmjs.org/

The base URL of the npm package registry. If `scope` is also specified,
it takes precedence.

### scope

Default: none

If specified, the user and login credentials given will be associated
with the specified scope. See `npm-scope(7)`. You can use both at the same time,
e.g.

    npm adduser --registry=http://myregistry.example.com --scope=@myco

This will set a registry for the given scope and login or create a user for
that registry at the same time.

## SEE ALSO

* npm-adduser(1)
* npm-registry(7)
* npm-config(1)
* npm-config(7)
* npmrc(5)
* npm-whoami(1)
