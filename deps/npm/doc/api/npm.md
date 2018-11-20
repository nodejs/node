npm(3) -- javascript package manager
====================================

## SYNOPSIS

    var npm = require("npm")
    npm.load([configObject, ]function (er, npm) {
      // use the npm object, now that it's loaded.

      npm.config.set(key, val)
      val = npm.config.get(key)

      console.log("prefix = %s", npm.prefix)

      npm.commands.install(["package"], cb)
    })

## VERSION

@VERSION@

## DESCRIPTION

This is the API documentation for npm.
To find documentation of the command line
client, see `npm(1)`.

Prior to using npm's commands, `npm.load()` must be called.  If you provide
`configObject` as an object map of top-level configs, they override the values
stored in the various config locations. In the npm command line client, this
set of configs is parsed from the command line options. Additional
configuration params are loaded from two configuration files. See
`npm-config(1)`, `npm-config(7)`, and `npmrc(5)` for more information.

After that, each of the functions are accessible in the
commands object: `npm.commands.<cmd>`.  See `npm-index(7)` for a list of
all possible commands.

All commands on the command object take an **array** of positional argument
**strings**. The last argument to any function is a callback. Some
commands take other optional arguments.

Configs cannot currently be set on a per function basis, as each call to
npm.config.set will change the value for *all* npm commands in that process.

To find API documentation for a specific command, run the `npm apihelp`
command.

## METHODS AND PROPERTIES

* `npm.load(configs, cb)`

    Load the configuration params, and call the `cb` function once the
    globalconfig and userconfig files have been loaded as well, or on
    nextTick if they've already been loaded.

* `npm.config`

    An object for accessing npm configuration parameters.

    * `npm.config.get(key)`
    * `npm.config.set(key, val)`
    * `npm.config.del(key)`

* `npm.dir` or `npm.root`

    The `node_modules` directory where npm will operate.

* `npm.prefix`

    The prefix where npm is operating.  (Most often the current working
    directory.)

* `npm.cache`

    The place where npm keeps JSON and tarballs it fetches from the
    registry (or uploads to the registry).

* `npm.tmp`

    npm's temporary working directory.

* `npm.deref`

    Get the "real" name for a command that has either an alias or
    abbreviation.

## MAGIC

For each of the methods in the `npm.commands` object, a method is added to the
npm object, which takes a set of positional string arguments rather than an
array and a callback.

If the last argument is a callback, then it will use the supplied
callback.  However, if no callback is provided, then it will print out
the error or results.

For example, this would work in a node repl:

    > npm = require("npm")
    > npm.load()  // wait a sec...
    > npm.install("dnode", "express")

Note that that *won't* work in a node program, since the `install`
method will get called before the configuration load is completed.

## ABBREVS

In order to support `npm ins foo` instead of `npm install foo`, the
`npm.commands` object has a set of abbreviations as well as the full
method names.  Use the `npm.deref` method to find the real name.

For example:

    var cmd = npm.deref("unp") // cmd === "unpublish"
