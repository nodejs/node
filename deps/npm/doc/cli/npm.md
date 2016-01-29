npm(1) -- javascript package manager
====================================

## SYNOPSIS

    npm <command> [args]

## VERSION

@VERSION@

## DESCRIPTION

npm is the package manager for the Node JavaScript platform.  It puts
modules in place so that node can find them, and manages dependency
conflicts intelligently.

It is extremely configurable to support a wide variety of use cases.
Most commonly, it is used to publish, discover, install, and develop node
programs.

Run `npm help` to get a list of available commands.

## INTRODUCTION

You probably got npm because you want to install stuff.

Use `npm install blerg` to install the latest version of "blerg".  Check out
`npm-install(1)` for more info.  It can do a lot of stuff.

Use the `npm search` command to show everything that's available.
Use `npm ls` to show everything you've installed.

## DEPENDENCIES

If a package references to another package with a git URL, npm depends
on a preinstalled git.

If one of the packages npm tries to install is a native node module and
requires compiling of C++ Code, npm will use
[node-gyp](https://github.com/TooTallNate/node-gyp) for that task.
For a Unix system, [node-gyp](https://github.com/TooTallNate/node-gyp)
needs Python, make and a buildchain like GCC. On Windows,
Python and Microsoft Visual Studio C++ is needed. Python 3 is
not supported by [node-gyp](https://github.com/TooTallNate/node-gyp).
For more information visit
[the node-gyp repository](https://github.com/TooTallNate/node-gyp) and
the [node-gyp Wiki](https://github.com/TooTallNate/node-gyp/wiki).

## DIRECTORIES

See `npm-folders(5)` to learn about where npm puts stuff.

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

## DEVELOPER USAGE

If you're using npm to develop and publish your code, check out the
following help topics:

* json:
  Make a package.json file.  See `package.json(5)`.
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

## CONFIGURATION

npm is extremely configurable.  It reads its configuration options from
5 places.

* Command line switches:  
  Set a config with `--key val`.  All keys take a value, even if they
  are booleans (the config parser doesn't know what the options are at
  the time of parsing.)  If no value is provided, then the option is set
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

See `npm-config(7)` for much much more information.

## CONTRIBUTIONS

Patches welcome!

* code:
  Read through `npm-coding-style(7)` if you plan to submit code.
  You don't have to agree with it, but you do have to follow it.
* docs:
  If you find an error in the documentation, edit the appropriate markdown
  file in the "doc" folder.  (Don't worry about generating the man page.)

Contributors are listed in npm's `package.json` file.  You can view them
easily by doing `npm view npm contributors`.

If you would like to contribute, but don't know what to work on, check
the issues list or ask on the mailing list.

* <https://github.com/npm/npm/issues>
* <npm-@googlegroups.com>

## BUGS

When you find issues, please report them:

* web:
  <https://github.com/npm/npm/issues>
* email:
  <npm-@googlegroups.com>

Be sure to include *all* of the output from the npm command that didn't work
as expected.  The `npm-debug.log` file is also helpful to provide.

You can also look for isaacs in #node.js on irc://irc.freenode.net.  He
will no doubt tell you to put the output in a gist or email.

## AUTHOR

[Isaac Z. Schlueter](http://blog.izs.me/) ::
[isaacs](https://github.com/isaacs/) ::
[@izs](http://twitter.com/izs) ::
<i@izs.me>

## SEE ALSO

* npm-help(1)
* npm-faq(7)
* README
* package.json(5)
* npm-install(1)
* npm-config(1)
* npm-config(7)
* npmrc(5)
* npm-index(7)
