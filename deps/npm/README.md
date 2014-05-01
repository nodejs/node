npm(1) -- node package manager
==============================
[![Build Status](https://img.shields.io/travis/npm/npm/master.svg)](https://travis-ci.org/npm/npm)
## SYNOPSIS

This is just enough info to get you up and running.

Much more info available via `npm help` once it's installed.

## IMPORTANT

**You need node v0.8 or higher to run this program.**

To install an old **and unsupported** version of npm that works on node 0.3
and prior, clone the git repo and dig through the old tags and branches.

## Super Easy Install

npm comes with node now.

### Windows Computers

Get the MSI.  npm is in it.

### Apple Macintosh Computers

Get the pkg.  npm is in it.

### Other Sorts of Unices

Run `make install`.  npm will be installed with node.

If you want a more fancy pants install (a different version, customized
paths, etc.) then read on.

## Fancy Install (Unix)

There's a pretty robust install script at
<https://www.npmjs.org/install.sh>.  You can download that and run it.

Here's an example using curl:

    curl -L https://npmjs.org/install.sh | sh

### Slightly Fancier

You can set any npm configuration params with that script:

    npm_config_prefix=/some/path sh install.sh

Or, you can run it in uber-debuggery mode:

    npm_debug=1 sh install.sh

### Even Fancier

Get the code with git.  Use `make` to build the docs and do other stuff.
If you plan on hacking on npm, `make link` is your friend.

If you've got the npm source code, you can also semi-permanently set
arbitrary config keys using the `./configure --key=val ...`, and then
run npm commands by doing `node cli.js <cmd> <args>`.  (This is helpful
for testing, or running stuff without actually installing npm itself.)

## Fancy Windows Install

You can download a zip file from <https://npmjs.org/dist/>, and unpack it
in the same folder where node.exe lives.

If that's not fancy enough for you, then you can fetch the code with
git, and mess with it directly.

## Installing on Cygwin

No.

## Permissions when Using npm to Install Other Stuff

**tl;dr**

* Use `sudo` for greater safety.  Or don't, if you prefer not to.
* npm will downgrade permissions if it's root before running any build
  scripts that package authors specified.

### More details...

As of version 0.3, it is recommended to run npm as root.
This allows npm to change the user identifier to the `nobody` user prior
to running any package build or test commands.

If you are not the root user, or if you are on a platform that does not
support uid switching, then npm will not attempt to change the userid.

If you would like to ensure that npm **always** runs scripts as the
"nobody" user, and have it fail if it cannot downgrade permissions, then
set the following configuration param:

    npm config set unsafe-perm false

This will prevent running in unsafe mode, even as non-root users.

## Uninstalling

So sad to see you go.

    sudo npm uninstall npm -g

Or, if that fails,

    sudo make uninstall

## More Severe Uninstalling

Usually, the above instructions are sufficient.  That will remove
npm, but leave behind anything you've installed.

If you would like to remove all the packages that you have installed,
then you can use the `npm ls` command to find them, and then `npm rm` to
remove them.

To remove cruft left behind by npm 0.x, you can use the included
`clean-old.sh` script file.  You can run it conveniently like this:

    npm explore npm -g -- sh scripts/clean-old.sh

npm uses two configuration files, one for per-user configs, and another
for global (every-user) configs.  You can view them by doing:

    npm config get userconfig   # defaults to ~/.npmrc
    npm config get globalconfig # defaults to /usr/local/etc/npmrc

Uninstalling npm does not remove configuration files by default.  You
must remove them yourself manually if you want them gone.  Note that
this means that future npm installs will not remember the settings that
you have chosen.

## Using npm Programmatically

If you would like to use npm programmatically, you can do that.
It's not very well documented, but it *is* rather simple.

Most of the time, unless you actually want to do all the things that
npm does, you should try using one of npm's dependencies rather than
using npm itself, if possible.

Eventually, npm will be just a thin cli wrapper around the modules
that it depends on, but for now, there are some things that you must
use npm itself to do.

    var npm = require("npm")
    npm.load(myConfigObject, function (er) {
      if (er) return handlError(er)
      npm.commands.install(["some", "args"], function (er, data) {
        if (er) return commandFailed(er)
        // command succeeded, and data might have some info
      })
      npm.on("log", function (message) { .... })
    })

The `load` function takes an object hash of the command-line configs.
The various `npm.commands.<cmd>` functions take an **array** of
positional argument **strings**.  The last argument to any
`npm.commands.<cmd>` function is a callback.  Some commands take other
optional arguments.  Read the source.

You cannot set configs individually for any single npm function at this
time.  Since `npm` is a singleton, any call to `npm.config.set` will
change the value for *all* npm commands in that process.

See `./bin/npm-cli.js` for an example of pulling config values off of the
command line arguments using nopt.  You may also want to check out `npm
help config` to learn about all the options you can set there.

## More Docs

Check out the [docs](https://www.npmjs.org/doc/),
especially the [faq](https://www.npmjs.org/doc/faq.html).

You can use the `npm help` command to read any of them.

If you're a developer, and you want to use npm to publish your program,
you should [read this](https://www.npmjs.org/doc/developers.html)

## Legal Stuff

"npm" and "The npm Registry" are owned by npm, Inc.
All rights reserved.  See the included LICENSE file for more details.

"Node.js" and "node" are trademarks owned by Joyent, Inc.

Modules published on the npm registry are not officially endorsed by
npm, Inc. or the Node.js project.

Data published to the npm registry is not part of npm itself, and is
the sole property of the publisher.  While every effort is made to
ensure accountability, there is absolutely no guarantee, warrantee, or
assertion expressed or implied as to the quality, fitness for a
specific purpose, or lack of malice in any given npm package.

If you have a complaint about a package in the public npm registry,
and cannot [resolve it with the package
owner](https://www.npmjs.org/doc/misc/npm-disputes.html), please email
<support@npmjs.com> and explain the situation.

Any data published to The npm Registry (including user account
information) may be removed or modified at the sole discretion of the
npm server administrators.

### In plainer english

npm is the property of npm, Inc.

If you publish something, it's yours, and you are solely accountable
for it.

If other people publish something, it's theirs.

Users can publish Bad Stuff.  It will be removed promptly if reported.
But there is no vetting process for published modules, and you use
them at your own risk.  Please inspect the source.

If you publish Bad Stuff, we may delete it from the registry, or even
ban your account in extreme cases.  So don't do that.

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

## SEE ALSO

* npm(1)
* npm-faq(7)
* npm-help(1)
* npm-index(7)
