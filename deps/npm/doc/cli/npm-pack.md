npm-pack(1) -- Create a tarball from a package
==============================================

## SYNOPSIS

    npm pack [[<@scope>/]<pkg>...] [--dry-run]

## DESCRIPTION

For anything that's installable (that is, a package folder, tarball,
tarball url, name@tag, name@version, name, or scoped name), this
command will fetch it to the cache, and then copy the tarball to the
current working directory as `<name>-<version>.tgz`, and then write
the filenames out to stdout.

If the same package is specified multiple times, then the file will be
overwritten the second time.

If no arguments are supplied, then npm packs the current package folder.

The `--dry-run` argument will do everything that pack usually does without
actually packing anything. Reports on what would have gone into the tarball.

## SEE ALSO

* npm-cache(1)
* npm-publish(1)
* npm-config(1)
* npm-config(7)
* npmrc(5)
