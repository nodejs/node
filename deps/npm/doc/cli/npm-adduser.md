npm-adduser(1) -- Add a registry user account
=============================================

## SYNOPSIS

    npm adduser [--registry=url] [--scope=@orgname] [--always-auth]

## DESCRIPTION

Create or verify a user named `<username>` in the specified registry, and
save the credentials to the `.npmrc` file. If no registry is specified,
the default registry will be used (see `npm-config(7)`).

The username, password, and email are read in from prompts.

You may use this command to change your email address, but not username
or password.

To reset your password, go to <https://www.npmjs.org/forgot>

You may use this command multiple times with the same user account to
authorize on a new machine.

`npm login` is an alias to `adduser` and behaves exactly the same way.

## CONFIGURATION

### registry

Default: http://registry.npmjs.org/

The base URL of the npm package registry. If `scope` is also specified,
this registry will only be used for packages with that scope. See `npm-scope(7)`.

### scope

Default: none

If specified, the user and login credentials given will be associated
with the specified scope. See `npm-scope(7)`. You can use both at the same time,
e.g.

    npm adduser --registry=http://myregistry.example.com --scope=@myco

This will set a registry for the given scope and login or create a user for
that registry at the same time.

### always-auth

Default: false

If specified, save configuration indicating that all requests to the given
registry should include authorization information. Useful for private
registries. Can be used with `--registry` and / or `--scope`, e.g.

    npm adduser --registry=http://private-registry.example.com --always-auth

This will ensure that all requests to that registry (including for tarballs)
include an authorization header. See `always-auth` in `npm-config(7)` for more
details on always-auth. Registry-specific configuaration of `always-auth` takes
precedence over any global configuration.

## SEE ALSO

* npm-registry(7)
* npm-config(1)
* npm-config(7)
* npmrc(5)
* npm-owner(1)
* npm-whoami(1)
