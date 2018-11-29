---
section: cli-commands 
title: npm
description: javascript package manager
---

# npm(1)

## javascript package manager

### Synopsis

```bash
npm <command> [args]
```

### Version

@VERSION@

### Description

npm is the package manager for the Node JavaScript platform.  It puts
modules in place so that node can find them, and manages dependency
conflicts intelligently.

It is extremely configurable to support a wide variety of use cases.
Most commonly, it is used to publish, discover, install, and develop node
programs.

Run `npm help` to get a list of available commands.

### Important

npm is configured to use npm, Inc.'s public registry at
https://registry.npmjs.org by default. Use of the npm public registry is
subject to terms of use available at https://www.npmjs.com/policies/terms.

You can configure npm to use any compatible registry you like, and even run
your own registry. Use of someone else's registry may be governed by their
terms of use.

### Introduction

You probably got npm because you want to install stuff.

Use `npm install blerg` to install the latest version of "blerg".  Check out
[`npm install`](/cli-commands/npm-install) for more info.  It can do a lot of stuff.

Use the `npm search` command to show everything that's available.
Use `npm ls` to show everything you've installed.

### Dependencies

If a package references to another package with a git URL, npm depends
on a preinstalled git.

If one of the packages npm tries to install is a native node module and
requires compiling of C++ Code, npm will use
[node-gyp](https://github.com/TooTallNate/node-gyp) for that task.
For a Unix system, [node-gyp](https://github.com/TooTallNate/node-gyp)
needs Python, make and a buildchain like GCC. On Windows,
Python and Microsoft Visual Studio C++ are needed. Python 3 is
not supported by [node-gyp](https://github.com/TooTallNate/node-gyp).
For more information visit
[the node-gyp repository](https://github.com/TooTallNate/node-gyp) and
the [node-gyp Wiki](https://github.com/TooTallNate/node-gyp/wiki).

### Directories

See [`folders`](/configuring-npm/folders) to learn about where npm puts stuff.

In particular, npm has two modes of operation:

* global mode:
  npm installs packages into the install prefix at
  `prefix/lib/node_modules` and bins are installed in `prefix/bin`.
* local mode:
  npm installs packages into the current project directory, which
  defaults to the current working directory.  Packages are installed to
  `./node_modules`, and bins are installed to `./node_modules/.bin`.

Local mode is the default.  Use `-g` or `--global` on any command to
operate in global mode instead.

### Developer Usage

If you're using npm to develop and publish your code, check out the
following help topics:

* json:
  Make a package.json file.  See [`package.json`](/configuring-npm/package.json).
* link:
  For linking your current working code into Node's path, so that you
  don't have to reinstall every time you make a change.  Use
  `npm link` to do this.
* install:
  It's a good idea to install things if you don't need the symbolic link.
  Especially, installing other peoples code from the registry is done via
  `npm install`
* adduser:
  Create an account or log in.  Credentials are stored in the
  user config file.
* publish:
  Use the `npm publish` command to upload your code to the registry.

#### Configuration

npm is extremely configurable.  It reads its configuration options from
5 places.

* Command line switches:
  Set a config with `--key val`.  All keys take a value, even if they
  are booleans (the config parser doesn't know what the options are at
  the time of parsing).  If no value is provided, then the option is set
  to boolean `true`.
* Environment Variables:
  Set any config by prefixing the name in an environment variable with
  `npm_config_`.  For example, `export npm_config_key=val`.
* User Configs:
  The file at $HOME/.npmrc is an ini-formatted list of configs.  If
  present, it is parsed.  If the `userconfig` option is set in the cli
  or env, then that will be used instead.
* Global Configs:
  The file found at ../etc/npmrc (from the node executable, by default
  this resolves to /usr/local/etc/npmrc) will be parsed if it is found.
  If the `globalconfig` option is set in the cli, env, or user config,
  then that file is parsed instead.
* Defaults:
  npm's default configuration options are defined in
  lib/utils/config-defs.js.  These must not be changed.

See [`config`](/using-npm/config) for much much more information.

### Contributions

Patches welcome!

If you would like to contribute, but don't know what to work on, read
the contributing guidelines and check the issues list.

* [CONTRIBUTING.md](https://github.com/npm/cli/blob/latest/CONTRIBUTING.md)
* [Bug tracker](https://github.com/npm/cli/issues)

### Bugs

When you find issues, please report them:

* web:
  <https://npm.community/c/bugs>

Be sure to follow the template and bug reporting guidelines. You can also ask
for help in the [support forum](https://npm.community/c/support) if you're
unsure if it's actually a bug or are having trouble coming up with a detailed
reproduction to report.

### Author

[Isaac Z. Schlueter](http://blog.izs.me/) ::
[isaacs](https://github.com/isaacs/) ::
[@izs](https://twitter.com/izs) ::
<i@izs.me>

### See Also
* [npm help](/cli-commands/npm-help)
* [package.json](/configuring-npm/package-json)
* [npm install](/cli-commands/npm-install)
* [npm config](/cli-commands/npm-config)
* [npmrc](/configuring-npm/npmrc)
