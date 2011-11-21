npm-adduser(1) -- Add a registry user account
=============================================

## SYNOPSIS

    npm adduser

## DESCRIPTION

Create or verify a user named `<username>` in the npm registry, and
save the credentials to the `.npmrc` file.

The username, password, and email are read in from prompts.

You may use this command to change your email address, but not username
or password.

To reset your password, go to <http://admin.npmjs.org/>

You may use this command multiple times with the same user account to
authorize on a new machine.

## CONFIGURATION

### registry

Default: http://registry.npmjs.org/

The base URL of the npm package registry.

## SEE ALSO

* npm-registry(1)
* npm-config(1)
* npm-owner(1)
* npm-whoami(1)
