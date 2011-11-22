npm-pack(3) -- Create a tarball from a package
==============================================

## SYNOPSIS

    npm.commands.pack([packages,] callback)

## DESCRIPTION

For anything that's installable (that is, a package folder, tarball,
tarball url, name@tag, name@version, or name), this command will fetch
it to the cache, and then copy the tarball to the current working
directory as `<name>-<version>.tgz`, and then write the filenames out to
stdout.

If the same package is specified multiple times, then the file will be
overwritten the second time.

If no arguments are supplied, then npm packs the current package folder.
