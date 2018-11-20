npm-removal(1) -- Cleaning the Slate
====================================

## SYNOPSIS

So sad to see you go.

    sudo npm uninstall npm -g

Or, if that fails, get the npm source code, and do:

    sudo make uninstall

## More Severe Uninstalling

Usually, the above instructions are sufficient.  That will remove
npm, but leave behind anything you've installed.

If that doesn't work, or if you require more drastic measures,
continue reading.

Note that this is only necessary for globally-installed packages.  Local
installs are completely contained within a project's `node_modules`
folder.  Delete that folder, and everything is gone (unless a package's
install script is particularly ill-behaved).

This assumes that you installed node and npm in the default place.  If
you configured node with a different `--prefix`, or installed npm with a
different prefix setting, then adjust the paths accordingly, replacing
`/usr/local` with your install prefix.

To remove everything npm-related manually:

    rm -rf /usr/local/{lib/node{,/.npm,_modules},bin,share/man}/npm*

If you installed things *with* npm, then your best bet is to uninstall
them with npm first, and then install them again once you have a
proper install.  This can help find any symlinks that are lying
around:

    ls -laF /usr/local/{lib/node{,/.npm},bin,share/man} | grep npm

Prior to version 0.3, npm used shim files for executables and node
modules.  To track those down, you can do the following:

    find /usr/local/{lib/node,bin} -exec grep -l npm \{\} \; ;

(This is also in the README file.)

## SEE ALSO

* README
* npm-rm(1)
* npm-prune(1)
