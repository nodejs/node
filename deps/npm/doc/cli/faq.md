npm-faq(1) -- Frequently Asked Questions
========================================

## Where can I find these docs in HTML?

<http://npmjs.org/doc/>, or run:

    npm config set viewer browser

to open these documents in your default web browser rather than `man`.

## It didn't work.

That's not really a question.

## Why didn't it work?

I don't know yet.

Read the error output, and if you can't figure out what it means,
do what it says and post a bug with all the information it asks for.

## Where does npm put stuff?

See `npm-folders(1)`

tl;dr:

* Use the `npm root` command to see where modules go, and the `npm bin`
  command to see where executables go
* Global installs are different from local installs.  If you install
  something with the `-g` flag, then its executables go in `npm bin -g`
  and its modules go in `npm root -g`.

## How do I install something on my computer in a central location?

Install it globally by tacking `-g` or `--global` to the command.  (This
is especially important for command line utilities that need to add
their bins to the global system `PATH`.)

## I installed something globally, but I can't `require()` it

Install it locally.

The global install location is a place for command-line utilities
to put their bins in the system `PATH`.  It's not for use with `require()`.

If you `require()` a module in your code, then that means it's a
dependency, and a part of your program.  You need to install it locally
in your program.

## Why can't npm just put everything in one place, like other package managers?

Not every change is an improvement, but every improvement is a change.
This would be like asking git to do network IO for every commit.  It's
not going to happen, because it's a terrible idea that causes more
problems than it solves.

It is much harder to avoid dependency conflicts without nesting
dependencies.  This is fundamental to the way that npm works, and has
proven to be an extremely successful approach.  See `npm-folders(1)` for
more details.

If you want a package to be installed in one place, and have all your
programs reference the same copy of it, then use the `npm link` command.
That's what it's for.  Install it globally, then link it into each
program that uses it.

## Whatever, I really want the old style 'everything global' style.

Write your own package manager, then.  It's not that hard.

npm will not help you do something that is known to be a bad idea.

## Should I check my `node_modules` folder into git?

Mikeal Rogers answered this question very well:

<http://www.mikealrogers.com/posts/nodemodules-in-git.html>

tl;dr

* Check `node_modules` into git for things you **deploy**, such as
  websites and apps.
* Do not check `node_modules` into git for libraries and modules
  intended to be reused.
* Use npm to manage dependencies in your dev environment, but not in
  your deployment scripts.

## Is it 'npm' or 'NPM' or 'Npm'?

npm should never be capitalized unless it is being displayed in a
location that is customarily all-caps (such as the title of man pages.)

## If 'npm' is an acronym, why is it never capitalized?

Contrary to the belief of many, "npm" is not in fact an abbreviation for
"Node Package Manager".  It is a recursive bacronymic abbreviation for
"npm is not an acronym".  (If it was "ninaa", then it would be an
acronym, and thus incorrectly named.)

"NPM", however, *is* an acronym (more precisely, a capitonym) for the
National Association of Pastoral Musicians.  You can learn more
about them at <http://npm.org/>.

In software, "NPM" is a Non-Parametric Mapping utility written by
Chris Rorden.  You can analyze pictures of brains with it.  Learn more
about the (capitalized) NPM program at <http://www.cabiatl.com/mricro/npm/>.

The first seed that eventually grew into this flower was a bash utility
named "pm", which was a shortened descendent of "pkgmakeinst", a
bash function that was used to install various different things on different
platforms, most often using Yahoo's `yinst`.  If `npm` was ever an
acronym for anything, it was `node pm` or maybe `new pm`.

So, in all seriousness, the "npm" project is named after its command-line
utility, which was organically selected to be easily typed by a right-handed
programmer using a US QWERTY keyboard layout, ending with the
right-ring-finger in a postition to type the `-` key for flags and
other command-line arguments.  That command-line utility is always
lower-case, though it starts most sentences it is a part of.

## How do I list installed packages?

`npm ls`

## How do I search for packages?

`npm search`

Arguments are greps.  `npm search jsdom` shows jsdom packages.

## How do I update npm?

    npm update npm -g

You can also update all outdated local packages by doing `npm update` without
any arguments, or global packages by doing `npm update -g`.

Occasionally, the version of npm will progress such that the current
version cannot be properly installed with the version that you have
installed already.  (Consider, if there is ever a bug in the `update`
command.)

In those cases, you can do this:

    curl http://npmjs.org/install.sh | sh

## What is a `package`?

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

## How do I install node with npm?

You don't.  Try one of these:

* <http://github.com/isaacs/nave>
* <http://github.com/visionmedia/n>
* <http://github.com/creationix/nvm>

## How can I use npm for development?

See `npm-developers(1)` and `npm-json(1)`.

You'll most likely want to `npm link` your development folder.  That's
awesomely handy.

To set up your own private registry, check out `npm-registry(1)`.

## Can I list a url as a dependency?

Yes.  It should be a url to a gzipped tarball containing a single folder
that has a package.json in its root, or a git url.
(See "what is a package?" above.)

## How do I symlink to a dev folder so I don't have to keep re-installing?

See `npm-link(1)`

## The package registry website.  What is that exactly?

See `npm-registry(1)`.

## What's up with the insecure channel warnings?

Until node 0.4.10, there were problems sending big files over HTTPS.  That
means that publishes go over HTTP by default in those versions of node.

## I forgot my password, and can't publish.  How do I reset it?

Go to <http://admin.npmjs.org/reset>.

## I get ECONNREFUSED a lot.  What's up?

Either the registry is down, or node's DNS isn't able to reach out.

To check if the registry is down, open up
<http://registry.npmjs.org/>
in a web browser.  This will also tell you if you are just unable to
access the internet for some reason.

If the registry IS down, let me know by emailing or posting an issue.
We'll have someone kick it or something.

## Who does npm?

`npm view npm author`

`npm view npm contributors`

## I have a question or request not addressed here. Where should I put it?

Discuss it on the mailing list, or post an issue.

* <npm-@googlegroups.com>
* <http://github.com/isaacs/npm/issues>

## Why does npm hate me?

npm is not capable of hatred.  It loves everyone, especially you.

## SEE ALSO

* npm(1)
* npm-developers(1)
* npm-json(1)
* npm-config(1)
* npm-folders(1)
