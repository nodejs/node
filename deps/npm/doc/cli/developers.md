npm-developers(1) -- Developer Guide
====================================

## DESCRIPTION

So, you've decided to use npm to develop (and maybe publish/deploy)
your project.

Fantastic!

There are a few things that you need to do above the simple steps
that your users will do to install your program.

## About These Documents

These are man pages.  If you install npm, you should be able to
then do `man npm-thing` to get the documentation on a particular
topic, or `npm help thing` to see the same information.

## What is a `package`

A package is:

* a) a folder containing a program described by a package.json file
* b) a gzipped tarball containing (a)
* c) a url that resolves to (b)
* d) a `<name>@<version>` that is published on the registry with (c)
* e) a `<name>@<tag>` that points to (d)
* f) a `<name>` that has a "latest" tag satisfying (e)
* g) a `git` url that, when cloned, results in (a).

Even if you never publish your package, you can still get a lot of
benefits of using npm if you just want to write a node program (a), and
perhaps if you also want to be able to easily install it elsewhere
after packing it up into a tarball (b).

Git urls can be of the form:

    git://github.com/user/project.git#commit-ish
    git+ssh://user@hostname:project.git#commit-ish
    git+http://user@hostname/project/blah.git#commit-ish
    git+https://user@hostname/project/blah.git#commit-ish

The `commit-ish` can be any tag, sha, or branch which can be supplied as
an argument to `git checkout`.  The default is `master`.

## The package.json File

You need to have a `package.json` file in the root of your project to do
much of anything with npm.  That is basically the whole interface.

See `npm-json(1)` for details about what goes in that file.  At the very
least, you need:

* name:
  This should be a string that identifies your project.  Please do not
  use the name to specify that it runs on node, or is in JavaScript.
  You can use the "engines" field to explicitly state the versions of
  node (or whatever else) that your program requires, and it's pretty
  well assumed that it's javascript.

  It does not necessarily need to match your github repository name.

  So, `node-foo` and `bar-js` are bad names.  `foo` or `bar` are better.

* version:
  A semver-compatible version.

* engines:
  Specify the versions of node (or whatever else) that your program
  runs on.  The node API changes a lot, and there may be bugs or new
  functionality that you depend on.  Be explicit.

* author:
  Take some credit.

* scripts:
  If you have a special compilation or installation script, then you
  should put it in the `scripts` hash.  You should definitely have at
  least a basic smoke-test command as the "scripts.test" field.
  See npm-scripts(1).

* main:
  If you have a single module that serves as the entry point to your
  program (like what the "foo" package gives you at require("foo")),
  then you need to specify that in the "main" field.

* directories:
  This is a hash of folders.  The best ones to include are "lib" and
  "doc", but if you specify a folder full of man pages in "man", then
  they'll get installed just like these ones.

You can use `npm init` in the root of your package in order to get you
started with a pretty basic package.json file.  See `npm-init(1)` for
more info.

## Keeping files *out* of your package

Use a `.npmignore` file to keep stuff out of your package.  If there's
no .npmignore file, but there *is* a .gitignore file, then npm will
ignore the stuff matched by the .gitignore file.  If you *want* to
include something that is excluded by your .gitignore file, you can
create an empty .npmignore file to override it.

## Link Packages

`npm link` is designed to install a development package and see the
changes in real time without having to keep re-installing it.  (You do
need to either re-link or `npm rebuild -g` to update compiled packages,
of course.)

More info at `npm-link(1)`.

## Before Publishing: Make Sure Your Package Installs and Works

**This is important.**

If you can not install it locally, you'll have
problems trying to publish it.  Or, worse yet, you'll be able to
publish it, but you'll be publishing a broken or pointless package.
So don't do that.

In the root of your package, do this:

    npm install . -g

That'll show you that it's working.  If you'd rather just create a symlink
package that points to your working directory, then do this:

    npm link

Use `npm ls -g` to see if it's there.

To test a local install, go into some other folder, and then do:

    cd ../some-other-folder
    npm install ../my-package

to install it locally into the node_modules folder in that other place.

Then go into the node-repl, and try using require("my-thing") to
bring in your module's main module.

## Create a User Account

Create a user with the adduser command.  It works like this:

    npm adduser

and then follow the prompts.

This is documented better in npm-adduser(1).

## Publish your package

This part's easy.  IN the root of your folder, do this:

    npm publish

You can give publish a url to a tarball, or a filename of a tarball,
or a path to a folder.

Note that pretty much **everything in that folder will be exposed**
by default.  So, if you have secret stuff in there, use a
`.npmignore` file to list out the globs to ignore, or publish
from a fresh checkout.

## Brag about it

Send emails, write blogs, blab in IRC.

Tell the world how easy it is to install your program!

## SEE ALSO

* npm-faq(1)
* npm(1)
* npm-init(1)
* npm-json(1)
* npm-scripts(1)
* npm-publish(1)
* npm-adduser(1)
* npm-registry(1)
