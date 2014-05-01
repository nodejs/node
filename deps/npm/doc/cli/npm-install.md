npm-install(1) -- Install a package
===================================

## SYNOPSIS

    npm install (with no args in a package dir)
    npm install <tarball file>
    npm install <tarball url>
    npm install <folder>
    npm install <name> [--save|--save-dev|--save-optional] [--save-exact]
    npm install <name>@<tag>
    npm install <name>@<version>
    npm install <name>@<version range>
    npm i (with any of the previous argument usage)

## DESCRIPTION

This command installs a package, and any packages that it depends on. If the
package has a shrinkwrap file, the installation of dependencies will be driven
by that. See npm-shrinkwrap(1).

A `package` is:

* a) a folder containing a program described by a package.json file
* b) a gzipped tarball containing (a)
* c) a url that resolves to (b)
* d) a `<name>@<version>` that is published on the registry (see `npm-registry(7)`) with (c)
* e) a `<name>@<tag>` that points to (d)
* f) a `<name>` that has a "latest" tag satisfying (e)
* g) a `<git remote url>` that resolves to (b)

Even if you never publish your package, you can still get a lot of
benefits of using npm if you just want to write a node program (a), and
perhaps if you also want to be able to easily install it elsewhere
after packing it up into a tarball (b).


* `npm install` (in package directory, no arguments):

    Install the dependencies in the local node_modules folder.

    In global mode (ie, with `-g` or `--global` appended to the command),
    it installs the current package context (ie, the current working
    directory) as a global package.

    By default, `npm install` will install all modules listed as
    dependencies. With the `--production` flag,
    npm will not install modules listed in `devDependencies`.

* `npm install <folder>`:

    Install a package that is sitting in a folder on the filesystem.

* `npm install <tarball file>`:

    Install a package that is sitting on the filesystem.  Note: if you just want
    to link a dev directory into your npm root, you can do this more easily by
    using `npm link`.

    Example:

          npm install ./package.tgz

* `npm install <tarball url>`:

    Fetch the tarball url, and then install it.  In order to distinguish between
    this and other options, the argument must start with "http://" or "https://"

    Example:

          npm install https://github.com/indexzero/forever/tarball/v0.5.6

* `npm install <name> [--save|--save-dev|--save-optional]`:

    Do a `<name>@<tag>` install, where `<tag>` is the "tag" config. (See
    `npm-config(7)`.)

    In most cases, this will install the latest version
    of the module published on npm.

    Example:

          npm install sax

    `npm install` takes 3 exclusive, optional flags which save or update
    the package version in your main package.json:

    * `--save`: Package will appear in your `dependencies`.

    * `--save-dev`: Package will appear in your `devDependencies`.

    * `--save-optional`: Package will appear in your `optionalDependencies`.

    When using any of the above options to save dependencies to your
    package.json, there is an additional, optional flag:

    * `--save-exact`: Saved dependencies will be configured with an
      exact version rather than using npm's default semver range
      operator.

    Examples:

          npm install sax --save
          npm install node-tap --save-dev
          npm install dtrace-provider --save-optional
          npm install readable-stream --save --save-exact


    **Note**: If there is a file or folder named `<name>` in the current
    working directory, then it will try to install that, and only try to
    fetch the package by name if it is not valid.

* `npm install <name>@<tag>`:

    Install the version of the package that is referenced by the specified tag.
    If the tag does not exist in the registry data for that package, then this
    will fail.

    Example:

          npm install sax@latest

* `npm install <name>@<version>`:

    Install the specified version of the package.  This will fail if the version
    has not been published to the registry.

    Example:

          npm install sax@0.1.1

* `npm install <name>@<version range>`:

    Install a version of the package matching the specified version range.  This
    will follow the same rules for resolving dependencies described in `package.json(5)`.

    Note that most version ranges must be put in quotes so that your shell will
    treat it as a single argument.

    Example:

          npm install sax@">=0.1.0 <0.2.0"

* `npm install <git remote url>`:

    Install a package by cloning a git remote url.  The format of the git
    url is:

          <protocol>://[<user>@]<hostname><separator><path>[#<commit-ish>]

    `<protocol>` is one of `git`, `git+ssh`, `git+http`, or
    `git+https`.  If no `<commit-ish>` is specified, then `master` is
    used.

    Examples:

          git+ssh://git@github.com:npm/npm.git#v1.0.27
          git+https://isaacs@github.com/npm/npm.git
          git://github.com/npm/npm.git#v1.0.27

You may combine multiple arguments, and even multiple types of arguments.
For example:

    npm install sax@">=0.1.0 <0.2.0" bench supervisor

The `--tag` argument will apply to all of the specified install targets. If a
tag with the given name exists, the tagged version is preferred over newer
versions.

The `--force` argument will force npm to fetch remote resources even if a
local copy exists on disk.

    npm install sax --force

The `--global` argument will cause npm to install the package globally
rather than locally.  See `npm-folders(5)`.

The `--link` argument will cause npm to link global installs into the
local space in some cases.

The `--no-bin-links` argument will prevent npm from creating symlinks for
any binaries the package might contain.

The `--no-optional` argument will prevent optional dependencies from
being installed.

The `--no-shrinkwrap` argument, which will ignore an available
shrinkwrap file and use the package.json instead.

The `--nodedir=/path/to/node/source` argument will allow npm to find the
node source code so that npm can compile native modules.

See `npm-config(7)`.  Many of the configuration params have some
effect on installation, since that's most of what npm does.

## ALGORITHM

To install a package, npm uses the following algorithm:

    install(where, what, family, ancestors)
    fetch what, unpack to <where>/node_modules/<what>
    for each dep in what.dependencies
      resolve dep to precise version
    for each dep@version in what.dependencies
        not in <where>/node_modules/<what>/node_modules/*
        and not in <family>
      add precise version deps to <family>
      install(<where>/node_modules/<what>, dep, family)

For this `package{dep}` structure: `A{B,C}, B{C}, C{D}`,
this algorithm produces:

    A
    +-- B
    `-- C
        `-- D

That is, the dependency from B to C is satisfied by the fact that A
already caused C to be installed at a higher level.

See npm-folders(5) for a more detailed description of the specific
folder structures that npm creates.

### Limitations of npm's Install Algorithm

There are some very rare and pathological edge-cases where a cycle can
cause npm to try to install a never-ending tree of packages.  Here is
the simplest case:

    A -> B -> A' -> B' -> A -> B -> A' -> B' -> A -> ...

where `A` is some version of a package, and `A'` is a different version
of the same package.  Because `B` depends on a different version of `A`
than the one that is already in the tree, it must install a separate
copy.  The same is true of `A'`, which must install `B'`.  Because `B'`
depends on the original version of `A`, which has been overridden, the
cycle falls into infinite regress.

To avoid this situation, npm flat-out refuses to install any
`name@version` that is already present anywhere in the tree of package
folder ancestors.  A more correct, but more complex, solution would be
to symlink the existing version into the new location.  If this ever
affects a real use-case, it will be investigated.

## SEE ALSO

* npm-folders(5)
* npm-update(1)
* npm-link(1)
* npm-rebuild(1)
* npm-scripts(7)
* npm-build(1)
* npm-config(1)
* npm-config(7)
* npmrc(5)
* npm-registry(7)
* npm-tag(1)
* npm-rm(1)
* npm-shrinkwrap(1)
