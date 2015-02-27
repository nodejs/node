npm-link(1) -- Symlink a package folder
=======================================

## SYNOPSIS

    npm link (in package folder)
    npm link [@<scope>/]<pkgname>
    npm ln (with any of the previous argument usage)

## DESCRIPTION

Package linking is a two-step process.

First, `npm link` in a package folder will create a globally-installed
symbolic link from `prefix/package-name` to the current folder (see
`npm-config(7)` for the value of `prefix`).

Next, in some other location, `npm link package-name` will create a
symlink from the local `node_modules` folder to the global symlink.

Note that `package-name` is taken from `package.json`,
not from directory name.

The package name can be optionally prefixed with a scope. See `npm-scope(7)`.
The scope must be preceded by an @-symbol and followed by a slash.

When creating tarballs for `npm publish`, the linked packages are
"snapshotted" to their current state by resolving the symbolic links.

This is handy for installing your own stuff, so that you can work on it and
test it iteratively without having to continually rebuild.

For example:

    cd ~/projects/node-redis    # go into the package directory
    npm link                    # creates global link
    cd ~/projects/node-bloggy   # go into some other package directory.
    npm link redis              # link-install the package

Now, any changes to ~/projects/node-redis will be reflected in
~/projects/node-bloggy/node_modules/node-redis/

You may also shortcut the two steps in one.  For example, to do the
above use-case in a shorter way:

    cd ~/projects/node-bloggy  # go into the dir of your main project
    npm link ../node-redis     # link the dir of your dependency

The second line is the equivalent of doing:

    (cd ../node-redis; npm link)
    npm link node-redis

That is, it first creates a global link, and then links the global
installation target into your project's `node_modules` folder.

If your linked package is scoped (see `npm-scope(7)`) your link command must
include that scope, e.g.

    npm link @myorg/privatepackage

## SEE ALSO

* npm-developers(7)
* npm-faq(7)
* package.json(5)
* npm-install(1)
* npm-folders(5)
* npm-config(1)
* npm-config(7)
* npmrc(5)
