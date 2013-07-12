npm-config(7) -- More than you probably want to know about npm configuration
============================================================================

## DESCRIPTION

npm gets its configuration values from 6 sources, in this priority:

### Command Line Flags

Putting `--foo bar` on the command line sets the `foo` configuration
parameter to `"bar"`.  A `--` argument tells the cli parser to stop
reading flags.  A `--flag` parameter that is at the *end* of the
command will be given the value of `true`.

### Environment Variables

Any environment variables that start with `npm_config_` will be
interpreted as a configuration parameter.  For example, putting
`npm_config_foo=bar` in your environment will set the `foo`
configuration parameter to `bar`.  Any environment configurations that
are not given a value will be given the value of `true`.  Config
values are case-insensitive, so `NPM_CONFIG_FOO=bar` will work the
same.

### npmrc Files

The three relevant files are:

* per-user config file (~/.npmrc)
* global config file ($PREFIX/npmrc)
* npm builtin config file (/path/to/npm/npmrc)

See npmrc(5) for more details.

### Default Configs

A set of configuration parameters that are internal to npm, and are
defaults if nothing else is specified.

## Shorthands and Other CLI Niceties

The following shorthands are parsed on the command-line:

* `-v`: `--version`
* `-h`, `-?`, `--help`, `-H`: `--usage`
* `-s`, `--silent`: `--loglevel silent`
* `-q`, `--quiet`: `--loglevel warn`
* `-d`: `--loglevel info`
* `-dd`, `--verbose`: `--loglevel verbose`
* `-ddd`: `--loglevel silly`
* `-g`: `--global`
* `-l`: `--long`
* `-m`: `--message`
* `-p`, `--porcelain`: `--parseable`
* `-reg`: `--registry`
* `-v`: `--version`
* `-f`: `--force`
* `-desc`: `--description`
* `-S`: `--save`
* `-D`: `--save-dev`
* `-O`: `--save-optional`
* `-B`: `--save-bundle`
* `-y`: `--yes`
* `-n`: `--yes false`
* `ll` and `la` commands: `ls --long`

If the specified configuration param resolves unambiguously to a known
configuration parameter, then it is expanded to that configuration
parameter.  For example:

    npm ls --par
    # same as:
    npm ls --parseable

If multiple single-character shorthands are strung together, and the
resulting combination is unambiguously not some other configuration
param, then it is expanded to its various component pieces.  For
example:

    npm ls -gpld
    # same as:
    npm ls --global --parseable --long --loglevel info

## Per-Package Config Settings

When running scripts (see `npm-scripts(7)`) the package.json "config"
keys are overwritten in the environment if there is a config param of
`<name>[@<version>]:<key>`.  For example, if the package.json has
this:

    { "name" : "foo"
    , "config" : { "port" : "8080" }
    , "scripts" : { "start" : "node server.js" } }

and the server.js is this:

    http.createServer(...).listen(process.env.npm_package_config_port)

then the user could change the behavior by doing:

    npm config set foo:port 80

See package.json(5) for more information.

## Config Settings

### always-auth

* Default: false
* Type: Boolean

Force npm to always require authentication when accessing the registry,
even for `GET` requests.

### bin-links

* Default: `true`
* Type: Boolean

Tells npm to create symlinks (or `.cmd` shims on Windows) for package
executables.

Set to false to have it not do this.  This can be used to work around
the fact that some file systems don't support symlinks, even on
ostensibly Unix systems.

### browser

* Default: OS X: `"open"`, Windows: `"start"`, Others: `"xdg-open"`
* Type: String

The browser that is called by the `npm docs` command to open websites.

### ca

* Default: The npm CA certificate
* Type: String or null

The Certificate Authority signing certificate that is trusted for SSL
connections to the registry.

Set to `null` to only allow "known" registrars, or to a specific CA cert
to trust only that specific signing authority.

See also the `strict-ssl` config.

### cache

* Default: Windows: `%APPDATA%\npm-cache`, Posix: `~/.npm`
* Type: path

The location of npm's cache directory.  See `npm-cache(1)`

### cache-lock-stale

* Default: 60000 (1 minute)
* Type: Number

The number of ms before cache folder lockfiles are considered stale.

### cache-lock-retries

* Default: 10
* Type: Number

Number of times to retry to acquire a lock on cache folder lockfiles.

### cache-lock-wait

* Default: 10000 (10 seconds)
* Type: Number

Number of ms to wait for cache lock files to expire.

### cache-max

* Default: Infinity
* Type: Number

The maximum time (in seconds) to keep items in the registry cache before
re-checking against the registry.

Note that no purging is done unless the `npm cache clean` command is
explicitly used, and that only GET requests use the cache.

### cache-min

* Default: 10
* Type: Number

The minimum time (in seconds) to keep items in the registry cache before
re-checking against the registry.

Note that no purging is done unless the `npm cache clean` command is
explicitly used, and that only GET requests use the cache.

### color

* Default: true on Posix, false on Windows
* Type: Boolean or `"always"`

If false, never shows colors.  If `"always"` then always shows colors.
If true, then only prints color codes for tty file descriptors.

### coverage

* Default: false
* Type: Boolean

A flag to tell test-harness to run with their coverage options enabled,
if they respond to the `npm_config_coverage` environment variable.

### depth

* Default: Infinity
* Type: Number

The depth to go when recursing directories for `npm ls` and
`npm cache ls`.

### description

* Default: true
* Type: Boolean

Show the description in `npm search`

### dev

* Default: false
* Type: Boolean

Install `dev-dependencies` along with packages.

Note that `dev-dependencies` are also installed if the `npat` flag is
set.

### editor

* Default: `EDITOR` environment variable if set, or `"vi"` on Posix,
  or `"notepad"` on Windows.
* Type: path

The command to run for `npm edit` or `npm config edit`.

### engine-strict

* Default: false
* Type: Boolean

If set to true, then npm will stubbornly refuse to install (or even
consider installing) any package that claims to not be compatible with
the current Node.js version.

### force

* Default: false
* Type: Boolean

Makes various commands more forceful.

* lifecycle script failure does not block progress.
* publishing clobbers previously published versions.
* skips cache when requesting from the registry.
* prevents checks against clobbering non-npm files.

### fetch-retries

* Default: 2
* Type: Number

The "retries" config for the `retry` module to use when fetching
packages from the registry.

### fetch-retry-factor

* Default: 10
* Type: Number

The "factor" config for the `retry` module to use when fetching
packages.

### fetch-retry-mintimeout

* Default: 10000 (10 seconds)
* Type: Number

The "minTimeout" config for the `retry` module to use when fetching
packages.

### fetch-retry-maxtimeout

* Default: 60000 (1 minute)
* Type: Number

The "maxTimeout" config for the `retry` module to use when fetching
packages.

### git

* Default: `"git"`
* Type: String

The command to use for git commands.  If git is installed on the
computer, but is not in the `PATH`, then set this to the full path to
the git binary.

### global

* Default: false
* Type: Boolean

Operates in "global" mode, so that packages are installed into the
`prefix` folder instead of the current working directory.  See
`npm-folders(7)` for more on the differences in behavior.

* packages are installed into the `{prefix}/lib/node_modules` folder, instead of the
  current working directory.
* bin files are linked to `{prefix}/bin`
* man pages are linked to `{prefix}/share/man`

### globalconfig

* Default: {prefix}/etc/npmrc
* Type: path

The config file to read for global config options.

### globalignorefile

* Default: {prefix}/etc/npmignore
* Type: path

The config file to read for global ignore patterns to apply to all users
and all projects.

If not found, but there is a "gitignore" file in the
same directory, then that will be used instead.

### group

* Default: GID of the current process
* Type: String or Number

The group to use when running package scripts in global mode as the root
user.

### https-proxy

* Default: the `HTTPS_PROXY` or `https_proxy` or `HTTP_PROXY` or
  `http_proxy` environment variables.
* Type: url

A proxy to use for outgoing https requests.

### user-agent

* Default: node/{process.version} {process.platform} {process.arch}
* Type: String

Sets a User-Agent to the request header

### ignore

* Default: ""
* Type: string

A white-space separated list of glob patterns of files to always exclude
from packages when building tarballs.

### init-module

* Default: ~/.npm-init.js
* Type: path

A module that will be loaded by the `npm init` command.  See the
documentation for the
[init-package-json](https://github.com/isaacs/init-package-json) module
for more information, or npm-init(1).

### init.version

* Default: "0.0.0"
* Type: semver

The value `npm init` should use by default for the package version.

### init.author.name

* Default: ""
* Type: String

The value `npm init` should use by default for the package author's name.

### init.author.email

* Default: ""
* Type: String

The value `npm init` should use by default for the package author's email.

### init.author.url

* Default: ""
* Type: String

The value `npm init` should use by default for the package author's homepage.

### json

* Default: false
* Type: Boolean

Whether or not to output JSON data, rather than the normal output.

This feature is currently experimental, and the output data structures
for many commands is either not implemented in JSON yet, or subject to
change.  Only the output from `npm ls --json` is currently valid.

### link

* Default: false
* Type: Boolean

If true, then local installs will link if there is a suitable globally
installed package.

Note that this means that local installs can cause things to be
installed into the global space at the same time.  The link is only done
if one of the two conditions are met:

* The package is not already installed globally, or
* the globally installed version is identical to the version that is
  being installed locally.

### loglevel

* Default: "http"
* Type: String
* Values: "silent", "win", "error", "warn", "http", "info", "verbose", "silly"

What level of logs to report.  On failure, *all* logs are written to
`npm-debug.log` in the current working directory.

Any logs of a higher level than the setting are shown.
The default is "http", which shows http, warn, and error output.

### logstream

* Default: process.stderr
* Type: Stream

This is the stream that is passed to the
[npmlog](https://github.com/isaacs/npmlog) module at run time.

It cannot be set from the command line, but if you are using npm
programmatically, you may wish to send logs to somewhere other than
stderr.

If the `color` config is set to true, then this stream will receive
colored output if it is a TTY.

### long

* Default: false
* Type: Boolean

Show extended information in `npm ls`

### message

* Default: "%s"
* Type: String

Commit message which is used by `npm version` when creating version commit.

Any "%s" in the message will be replaced with the version number.

### node-version

* Default: process.version
* Type: semver or false

The node version to use when checking package's "engines" hash.

### npat

* Default: false
* Type: Boolean

Run tests on installation and report results to the
`npaturl`.

### npaturl

* Default: Not yet implemented
* Type: url

The url to report npat test results.

### onload-script

* Default: false
* Type: path

A node module to `require()` when npm loads.  Useful for programmatic
usage.

### optional

* Default: true
* Type: Boolean

Attempt to install packages in the `optionalDependencies` hash.  Note
that if these packages fail to install, the overall installation
process is not aborted.

### parseable

* Default: false
* Type: Boolean

Output parseable results from commands that write to
standard output.

### prefix

* Default: see npm-folders(7)
* Type: path

The location to install global items.  If set on the command line, then
it forces non-global commands to run in the specified folder.

### production

* Default: false
* Type: Boolean

Set to true to run in "production" mode.

1. devDependencies are not installed at the topmost level when running
   local `npm install` without any arguments.
2. Set the NODE_ENV="production" for lifecycle scripts.

### proprietary-attribs

* Default: true
* Type: Boolean

Whether or not to include proprietary extended attributes in the
tarballs created by npm.

Unless you are expecting to unpack package tarballs with something other
than npm -- particularly a very outdated tar implementation -- leave
this as true.

### proxy

* Default: `HTTP_PROXY` or `http_proxy` environment variable, or null
* Type: url

A proxy to use for outgoing http requests.

### rebuild-bundle

* Default: true
* Type: Boolean

Rebuild bundled dependencies after installation.

### registry

* Default: https://registry.npmjs.org/
* Type: url

The base URL of the npm package registry.

### rollback

* Default: true
* Type: Boolean

Remove failed installs.

### save

* Default: false
* Type: Boolean

Save installed packages to a package.json file as dependencies.

When used with the `npm rm` command, it removes it from the dependencies
hash.

Only works if there is already a package.json file present.

### save-bundle

* Default: false
* Type: Boolean

If a package would be saved at install time by the use of `--save`,
`--save-dev`, or `--save-optional`, then also put it in the
`bundleDependencies` list.

When used with the `npm rm` command, it removes it from the
bundledDependencies list.

### save-dev

* Default: false
* Type: Boolean

Save installed packages to a package.json file as devDependencies.

When used with the `npm rm` command, it removes it from the devDependencies
hash.

Only works if there is already a package.json file present.

### save-optional

* Default: false
* Type: Boolean

Save installed packages to a package.json file as optionalDependencies.

When used with the `npm rm` command, it removes it from the devDependencies
hash.

Only works if there is already a package.json file present.

### searchopts

* Default: ""
* Type: String

Space-separated options that are always passed to search.

### searchexclude

* Default: ""
* Type: String

Space-separated options that limit the results from search.

### searchsort

* Default: "name"
* Type: String
* Values: "name", "-name", "date", "-date", "description",
  "-description", "keywords", "-keywords"

Indication of which field to sort search results by.  Prefix with a `-`
character to indicate reverse sort.

### shell

* Default: SHELL environment variable, or "bash" on Posix, or "cmd" on
  Windows
* Type: path

The shell to run for the `npm explore` command.

### shrinkwrap

* Default: true
* Type: Boolean

If set to false, then ignore `npm-shrinkwrap.json` files when
installing.

### sign-git-tag

* Default: false
* Type: Boolean

If set to true, then the `npm version` command will tag the version
using `-s` to add a signature.

Note that git requires you to have set up GPG keys in your git configs
for this to work properly.

### strict-ssl

* Default: true
* Type: Boolean

Whether or not to do SSL key validation when making requests to the
registry via https.

See also the `ca` config.

### tag

* Default: latest
* Type: String

If you ask npm to install a package and don't tell it a specific version, then
it will install the specified tag.

Also the tag that is added to the package@version specified by the `npm
tag` command, if no explicit tag is given.

### tmp

* Default: TMPDIR environment variable, or "/tmp"
* Type: path

Where to store temporary files and folders.  All temp files are deleted
on success, but left behind on failure for forensic purposes.

### unicode

* Default: true
* Type: Boolean

When set to true, npm uses unicode characters in the tree output.  When
false, it uses ascii characters to draw trees.

### unsafe-perm

* Default: false if running as root, true otherwise
* Type: Boolean

Set to true to suppress the UID/GID switching when running package
scripts.  If set explicitly to false, then installing as a non-root user
will fail.

### usage

* Default: false
* Type: Boolean

Set to show short usage output (like the -H output)
instead of complete help when doing `npm-help(1)`.

### user

* Default: "nobody"
* Type: String or Number

The UID to set to when running package scripts as root.

### username

* Default: null
* Type: String

The username on the npm registry.  Set with `npm adduser`

### userconfig

* Default: ~/.npmrc
* Type: path

The location of user-level configuration settings.

### userignorefile

* Default: ~/.npmignore
* Type: path

The location of a user-level ignore file to apply to all packages.

If not found, but there is a .gitignore file in the same directory, then
that will be used instead.

### umask

* Default: 022
* Type: Octal numeric string

The "umask" value to use when setting the file creation mode on files
and folders.

Folders and executables are given a mode which is `0777` masked against
this value.  Other files are given a mode which is `0666` masked against
this value.  Thus, the defaults are `0755` and `0644` respectively.

### version

* Default: false
* Type: boolean

If true, output the npm version and exit successfully.

Only relevant when specified explicitly on the command line.

### versions

* Default: false
* Type: boolean

If true, output the npm version as well as node's `process.versions`
hash, and exit successfully.

Only relevant when specified explicitly on the command line.

### viewer

* Default: "man" on Posix, "browser" on Windows
* Type: path

The program to use to view help content.

Set to `"browser"` to view html help content in the default web browser.

### yes

* Default: null
* Type: Boolean or null

If set to `null`, then prompt the user for responses in some
circumstances.

If set to `true`, then answer "yes" to any prompt.  If set to `false`
then answer "no" to any prompt.

## SEE ALSO

* npm-config(1)
* npm-config(7)
* npmrc(5)
* npm-scripts(7)
* npm-folders(5)
* npm(1)
