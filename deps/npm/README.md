npm(1) -- a JavaScript package manager
==============================

[![Build Status](https://img.shields.io/travis/npm/npm/latest.svg)](https://travis-ci.org/npm/npm)

## SYNOPSIS

This is just enough info to get you up and running.

Much more info will be available via `npm help` once it's installed.

## IMPORTANT

**You need node v6 or higher to run this program.**

To install an old **and unsupported** version of npm that works on node v5
and prior, clone the git repo and dig through the old tags and branches.

**npm is configured to use npm, Inc.'s public package registry at
<https://registry.npmjs.org> by default.**

You can configure npm to use any compatible registry you
like, and even run your own registry. Check out the [doc on
registries](https://docs.npmjs.com/misc/registry).

Use of someone else's registry may be governed by terms of use. The
terms of use for the default public registry are available at
<https://www.npmjs.com>.

## Super Easy Install

npm is bundled with [node](https://nodejs.org/en/download/).

### Windows Computers

[Get the MSI](https://nodejs.org/en/download/).  npm is in it.

### Apple Macintosh Computers

[Get the pkg](https://nodejs.org/en/download/).  npm is in it.

### Other Sorts of Unices

Run `make install`.  npm will be installed with node.

If you want a more fancy pants install (a different version, customized
paths, etc.) then read on.

## Fancy Install (Unix)

There's a pretty robust install script at
<https://www.npmjs.com/install.sh>.  You can download that and run it.

Here's an example using curl:

```sh
curl -L https://www.npmjs.com/install.sh | sh
```

### Slightly Fancier

You can set any npm configuration params with that script:

```sh
npm_config_prefix=/some/path sh install.sh
```

Or, you can run it in uber-debuggery mode:

```sh
npm_debug=1 sh install.sh
```

### Even Fancier

Get the code with git.  Use `make` to build the docs and do other stuff.
If you plan on hacking on npm, `make link` is your friend.

If you've got the npm source code, you can also semi-permanently set
arbitrary config keys using the `./configure --key=val ...`, and then
run npm commands by doing `node bin/npm-cli.js <command> <args>`.  (This is helpful
for testing, or running stuff without actually installing npm itself.)

## Windows Install or Upgrade

Many improvements for Windows users have been made in npm 3 - you will have a better
experience if you run a recent version of npm. To upgrade, either use [Microsoft's
upgrade tool](https://github.com/felixrieseberg/npm-windows-upgrade),
[download a new version of Node](https://nodejs.org/en/download/),
or follow the Windows upgrade instructions in the
[npm Troubleshooting Guide](./TROUBLESHOOTING.md).

If that's not fancy enough for you, then you can fetch the code with
git, and mess with it directly.

## Installing on Cygwin

No.

## Uninstalling

So sad to see you go.

```sh
sudo npm uninstall npm -g
```
Or, if that fails,

```sh
sudo make uninstall
```

## More Severe Uninstalling

Usually, the above instructions are sufficient.  That will remove
npm, but leave behind anything you've installed.

If you would like to remove all the packages that you have installed,
then you can use the `npm ls` command to find them, and then `npm rm` to
remove them.

To remove cruft left behind by npm 0.x, you can use the included
`clean-old.sh` script file.  You can run it conveniently like this:

```sh
npm explore npm -g -- sh scripts/clean-old.sh
```

npm uses two configuration files, one for per-user configs, and another
for global (every-user) configs.  You can view them by doing:

```sh
npm config get userconfig   # defaults to ~/.npmrc
npm config get globalconfig # defaults to /usr/local/etc/npmrc
```

Uninstalling npm does not remove configuration files by default.  You
must remove them yourself manually if you want them gone.  Note that
this means that future npm installs will not remember the settings that
you have chosen.

## More Docs

Check out the [docs](https://docs.npmjs.com/).

You can use the `npm help` command to read any of them.

If you're a developer, and you want to use npm to publish your program,
you should [read this](https://docs.npmjs.com/misc/developers).

## BUGS

When you find issues, please report them:

* web:
  <https://npm.community/c/bugs>

Be sure to include *all* of the output from the npm command that didn't work
as expected.  The `npm-debug.log` file is also helpful to provide.

You can also find npm people in `#npm` on https://package.community/ or
[on Twitter](https://twitter.com/npm_support).  Whoever responds will no
doubt tell you to put the output in a gist or email.

## SEE ALSO

* npm(1)
* npm-help(1)
* npm-index(7)
