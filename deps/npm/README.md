npm(1) -- node package manager
==============================

## SYNOPSIS

This is just enough info to get you up and running.

Much more info available via `npm help` once it's installed.

## IMPORTANT

**You need node v0.4 or higher to run this program.**

To install an old **and unsupported** version of npm that works on node 0.3
and prior, clone the git repo and dig through the old tags and branches.

## Simple Install (Unix only, sorry)

To install npm with one command, do this:

    curl http://npmjs.org/install.sh | sh

To skip the npm 0.x cleanup, do this:

    curl http://npmjs.org/install.sh | clean=no sh

To say "yes" to the 0.x cleanup, but skip the prompt:

    curl http://npmjs.org/install.sh | clean=yes sh

If you get permission errors, see the section below, entitled
"Permission Errors on Installation".

## Installing on Windows -- Experimental

Yes, this sucks.  A convenient one-liner is coming soon.

### Step 1: Drop the node.exe somewhere

You will probably need the latest version of node, **at least** version
`0.5.8` or higher.  You can get it from
<http://nodejs.org/dist/v0.5.8/node.exe>.

### Step 2 (optional): Update the %PATH% environment variable

Update your `%PATH%` environment variable in System Properties:
Advanced: Environment, so that it includes the `bin` folder you chose.
The entries are separated by semicolons.

You *may* be able to do this from the command line using `set` and
`setx`.  `cd` into the `bin` folder you created in step 1, and do this:

    set path=%PATH%;%CD%
    setx path "%PATH%"

This will have the added advantage that you'll be able to simply type
`npm` or `node` in any project folder to access those commands.

If you decide not to update the PATH, and put the node.exe file in
`C:\node\node.exe`, then the npm executable will end up `C:\node\npm.cmd`,
and you'll have to type `C:\node\npm <command>` to use it.

### Step 3: Install git

If you don't already have git,
[install it](https://git.wiki.kernel.org/index.php/MSysGit:InstallMSysGit).

Run `git --version` to make sure that it's at least version 1.7.6.

### Step 4: install npm

Lastly, **after** node.exe, git, and your %PATH% have *all* been set up
properly, install npm itself:

    git config --system http.sslcainfo /bin/curl-ca-bundle.crt
    git clone --recursive git://github.com/isaacs/npm.git
    cd npm
    node cli.js install npm -gf

## Permission Errors (`EACCES` or `EACCESS`) on Installation

On Windows, you may need to run the command prompt in elevated
permission mode.  (Right-click on cmd.exe, Run as Administrator.)

On Unix, you may need to run as root, or use `sudo`.

**Note**: You would need to `sudo` the `sh`, **not** the `curl`.  Fetching
stuff from the internet typically doesn't require elevated permissions.
Running it might.

I highly recommend that you first download the file, and make sure that
it is what you expect, and *then* run it.

    curl -O http://npmjs.org/install.sh
    # inspect file..
    sudo sh install.sh

## Installing on Cygwin

No.

## Dev Install

To install the latest **unstable** development version from git:

    git clone https://github.com/isaacs/npm.git
    cd npm
    git submodule update --init --recursive
    sudo make install     # (or: `node cli.js install -gf`)

If you're sitting in the code folder reading this document in your
terminal, then you've already got the code.  Just do:

    git submodule update --init --recursive
    sudo make install

and npm will install itself.

If you don't have make, and don't have curl or git, and ALL you have is
this code and node, you can probably do this:

    git submodule update --init --recursive
    sudo node ./cli.js install -g

Note that github tarballs **do not contain submodules**, so
those won't work.  You'll have to also fetch the appropriate submodules
listed in the .gitmodules file.

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

Check out the [docs](http://npmjs.org/doc/),
especially the
[faq](http://npmjs.org/doc/faq.html).

You can use the `npm help` command to read any of them.

If you're a developer, and you want to use npm to publish your program,
you should
[read this](http://npmjs.org/doc/developers.html)

## Legal Stuff

"npm" and "the npm registry" are owned by Isaac Z. Schlueter.  All
rights not explicitly granted in the MIT license are reserved. See the
included LICENSE file for more details.

"Node.js" and "node" are trademarks owned by Joyent, Inc.  npm is not
officially part of the Node.js project, and is neither owned by nor
officially affiliated with Joyent, Inc.

The packages in the npm registry are not part of npm itself, and are the
sole property of their respective maintainers.  While every effort is
made to ensure accountability, there is absolutely no guarantee,
warrantee, or assertion made as to the quality, fitness for a specific
purpose, or lack of malice in any given npm package.  Modules
published on the npm registry are not affiliated with or endorsed by
Joyent, Inc., Isaac Z. Schlueter, Ryan Dahl, or the Node.js project.

If you have a complaint about a package in the npm registry, and cannot
resolve it with the package owner, please express your concerns to
Isaac Z. Schlueter at <i@izs.me>.

### In plain english

This is mine; not my employer's, not Node's, not Joyent's, not Ryan
Dahl's.

If you publish something, it's yours, and you are solely accountable
for it.  Not me, not Node, not Joyent, not Ryan Dahl.

If other people publish something, it's theirs.  Not mine, not Node's,
not Joyent's, not Ryan Dahl's.

Yes, you can publish something evil.  It will be removed promptly if
reported, and we'll lose respect for you.  But there is no vetting
process for published modules.

If this concerns you, inspect the source before using packages.

## SEE ALSO

* npm(1)
* npm-faq(1)
* npm-help(1)
* npm-index(1)
