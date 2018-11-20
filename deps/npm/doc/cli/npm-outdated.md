npm-outdated(1) -- Check for outdated packages
==============================================

## SYNOPSIS

    npm outdated [<name> [<name> ...]]

## DESCRIPTION

This command will check the registry to see if any (or, specific) installed
packages are currently outdated.

The resulting field 'wanted' shows the latest version according to the
version specified in the package.json, the field 'latest' the very latest
version of the package.

## CONFIGURATION

### json

* Default: false
* Type: Boolean

Show information in JSON format.

### long

* Default: false
* Type: Boolean

Show extended information.

### parseable

* Default: false
* Type: Boolean

Show parseable output instead of tree view.

### global

* Default: false
* Type: Boolean

Check packages in the global install prefix instead of in the current
project.

### depth

* Type: Int

Max depth for checking dependency tree.

## SEE ALSO

* npm-update(1)
* npm-registry(7)
* npm-folders(5)
