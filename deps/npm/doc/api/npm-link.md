npm-link(3) -- Symlink a package folder
=======================================

## SYNOPSIS

    npm.command.link(callback)
    npm.command.link(packages, callback)

## DESCRIPTION

Package linking is a two-step process.

Without parameters, link will create a globally-installed
symbolic link from `prefix/package-name` to the current folder.

With a parameters, link will create a symlink from the local `node_modules`
folder to the global symlink.

When creating tarballs for `npm publish`, the linked packages are
"snapshotted" to their current state by resolving the symbolic links.

This is
handy for installing your own stuff, so that you can work on it and test it
iteratively without having to continually rebuild.

For example:

    npm.commands.link(cb)           # creates global link from the cwd
                                    # (say redis package)
    npm.commands.link('redis', cb)  # link-install the package

Now, any changes to the redis package will be reflected in
the package in the current working directory
