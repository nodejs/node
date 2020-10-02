# mkdirp-infer-owner

[`mkdirp`](http://npm.im/mkdirp), but chown to the owner of the containing
folder if possible and necessary.

That is, on Windows and when running as non-root, it's exactly the same as
[`mkdirp`](http://npm.im/mkdirp).

When running as root on non-Windows systems, it uses
[`infer-owner`](http://npm.im/infer-owner) to find the owner of the
containing folder, and then [`chownr`](http://npm.im/chownr) to set the
ownership of the created folder to that same uid/gid.

This is used by [npm](http://npm.im/npm) to prevent root-owned files and
folders from showing up in your home directory (either in `node_modules` or
in the `~/.npm` cache) when running as root.
