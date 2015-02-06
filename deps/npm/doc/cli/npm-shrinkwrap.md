npm-shrinkwrap(1) -- Lock down dependency versions
=====================================================

## SYNOPSIS

    npm shrinkwrap

## DESCRIPTION

This command locks down the versions of a package's dependencies so
that you can control exactly which versions of each dependency will be
used when your package is installed. The "package.json" file is still
required if you want to use "npm install".

By default, "npm install" recursively installs the target's
dependencies (as specified in package.json), choosing the latest
available version that satisfies the dependency's semver pattern. In
some situations, particularly when shipping software where each change
is tightly managed, it's desirable to fully specify each version of
each dependency recursively so that subsequent builds and deploys do
not inadvertently pick up newer versions of a dependency that satisfy
the semver pattern. Specifying specific semver patterns in each
dependency's package.json would facilitate this, but that's not always
possible or desirable, as when another author owns the npm package.
It's also possible to check dependencies directly into source control,
but that may be undesirable for other reasons.

As an example, consider package A:

    {
      "name": "A",
      "version": "0.1.0",
      "dependencies": {
        "B": "<0.1.0"
      }
    }

package B:

    {
      "name": "B",
      "version": "0.0.1",
      "dependencies": {
        "C": "<0.1.0"
      }
    }

and package C:

    {
      "name": "C,
      "version": "0.0.1"
    }

If these are the only versions of A, B, and C available in the
registry, then a normal "npm install A" will install:

    A@0.1.0
    `-- B@0.0.1
        `-- C@0.0.1

However, if B@0.0.2 is published, then a fresh "npm install A" will
install:

    A@0.1.0
    `-- B@0.0.2
        `-- C@0.0.1

assuming the new version did not modify B's dependencies. Of course,
the new version of B could include a new version of C and any number
of new dependencies. If such changes are undesirable, the author of A
could specify a dependency on B@0.0.1. However, if A's author and B's
author are not the same person, there's no way for A's author to say
that he or she does not want to pull in newly published versions of C
when B hasn't changed at all.

In this case, A's author can run

    npm shrinkwrap

This generates npm-shrinkwrap.json, which will look something like this:

    {
      "name": "A",
      "version": "0.1.0",
      "dependencies": {
        "B": {
          "version": "0.0.1",
          "dependencies": {
            "C": {
              "version": "0.0.1"
            }
          }
        }
      }
    }

The shrinkwrap command has locked down the dependencies based on
what's currently installed in node_modules.  When "npm install"
installs a package with a npm-shrinkwrap.json file in the package
root, the shrinkwrap file (rather than package.json files) completely
drives the installation of that package and all of its dependencies
(recursively).  So now the author publishes A@0.1.0, and subsequent
installs of this package will use B@0.0.1 and C@0.0.1, regardless the
dependencies and versions listed in A's, B's, and C's package.json
files.


### Using shrinkwrapped packages

Using a shrinkwrapped package is no different than using any other
package: you can "npm install" it by hand, or add a dependency to your
package.json file and "npm install" it.

### Building shrinkwrapped packages

To shrinkwrap an existing package:

1. Run "npm install" in the package root to install the current
   versions of all dependencies.
2. Validate that the package works as expected with these versions.
3. Run "npm shrinkwrap", add npm-shrinkwrap.json to git, and publish
   your package.

To add or update a dependency in a shrinkwrapped package:

1. Run "npm install" in the package root to install the current
   versions of all dependencies.
2. Add or update dependencies. "npm install" each new or updated
   package individually and then update package.json.  Note that they
   must be explicitly named in order to be installed: running `npm
   install` with no arguments will merely reproduce the existing
   shrinkwrap.
3. Validate that the package works as expected with the new
   dependencies.
4. Run "npm shrinkwrap", commit the new npm-shrinkwrap.json, and
   publish your package.

You can use npm-outdated(1) to view dependencies with newer versions
available.

### Other Notes

A shrinkwrap file must be consistent with the package's package.json
file. "npm shrinkwrap" will fail if required dependencies are not
already installed, since that would result in a shrinkwrap that
wouldn't actually work. Similarly, the command will fail if there are
extraneous packages (not referenced by package.json), since that would
indicate that package.json is not correct.

Since "npm shrinkwrap" is intended to lock down your dependencies for
production use, `devDependencies` will not be included unless you
explicitly set the `--dev` flag when you run `npm shrinkwrap`.  If
installed `devDependencies` are excluded, then npm will print a
warning.  If you want them to be installed with your module by
default, please consider adding them to `dependencies` instead.

If shrinkwrapped package A depends on shrinkwrapped package B, B's
shrinkwrap will not be used as part of the installation of A. However,
because A's shrinkwrap is constructed from a valid installation of B
and recursively specifies all dependencies, the contents of B's
shrinkwrap will implicitly be included in A's shrinkwrap.

### Caveats

If you wish to lock down the specific bytes included in a package, for
example to have 100% confidence in being able to reproduce a
deployment or build, then you ought to check your dependencies into
source control, or pursue some other mechanism that can verify
contents rather than versions.

## SEE ALSO

* npm-install(1)
* package.json(5)
* npm-ls(1)
