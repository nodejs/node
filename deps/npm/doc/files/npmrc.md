npmrc(5) -- The npm config files
================================

## DESCRIPTION

npm gets its config settings from the command line, environment
variables, and `npmrc` files.

The `npm config` command can be used to update and edit the contents
of the user and global npmrc files.

For a list of available configuration options, see npm-config(7).

## FILES

The three relevant files are:

* per-user config file (~/.npmrc)
* global config file ($PREFIX/npmrc)
* npm builtin config file (/path/to/npm/npmrc)

All npm config files are an ini-formatted list of `key = value`
parameters.  Environment variables can be replaced using
`${VARIABLE_NAME}`. For example:

    prefix = ${HOME}/.npm-packages

Each of these files is loaded, and config options are resolved in
priority order.  For example, a setting in the userconfig file would
override the setting in the globalconfig file.

### Per-user config file

`$HOME/.npmrc` (or the `userconfig` param, if set in the environment
or on the command line)

### Global config file

`$PREFIX/etc/npmrc` (or the `globalconfig` param, if set above):
This file is an ini-file formatted list of `key = value` parameters.
Environment variables can be replaced as above.

### Built-in config file

`path/to/npm/itself/npmrc`

This is an unchangeable "builtin" configuration file that npm keeps
consistent across updates.  Set fields in here using the `./configure`
script that comes with npm.  This is primarily for distribution
maintainers to override default configs in a standard and consistent
manner.

## SEE ALSO

* npm-folders(5)
* npm-config(1)
* npm-config(7)
* package.json(5)
* npm(1)
