npm-faq(7) -- Frequently Asked Questions
========================================

## Where can I find these docs in HTML?

<https://www.npmjs.org/doc/>, or run:

    npm config set viewer browser

to open these documents in your default web browser rather than `man`.

## It didn't work.

That's not really a question.

## Why didn't it work?

I don't know yet.

Read the error output, and if you can't figure out what it means,
do what it says and post a bug with all the information it asks for.

## Where does npm put stuff?

See `npm-folders(5)`

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
proven to be an extremely successful approach.  See `npm-folders(5)` for
more details.

If you want a package to be installed in one place, and have all your
programs reference the same copy of it, then use the `npm link` command.
That's what it's for.  Install it globally, then link it into each
program that uses it.

## Whatever, I really want the old style 'everything global' style.

Write your own package manager.  You could probably even wrap up `npm`
in a shell script if you really wanted to.

npm will not help you do something that is known to be a bad idea.

## Should I check my `node_modules` folder into git?

Usually, no. Allow npm to resolve dependencies for your packages.

For packages you **deploy**, such as websites and apps,
you should use npm shrinkwrap to lock down your full dependency tree:

https://www.npmjs.org/doc/cli/npm-shrinkwrap.html

If you are paranoid about depending on the npm ecosystem,
you should run a private npm mirror or a private cache.

If you want 100% confidence in being able to reproduce the specific bytes
included in a deployment, you should use an additional mechanism that can
verify contents rather than versions. For example,
Amazon machine images, DigitalOcean snapshots, Heroku slugs, or simple tarballs.

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

    npm install npm -g

You can also update all outdated local packages by doing `npm update` without
any arguments, or global packages by doing `npm update -g`.

Occasionally, the version of npm will progress such that the current
version cannot be properly installed with the version that you have
installed already.  (Consider, if there is ever a bug in the `update`
command.)

In those cases, you can do this:

    curl https://www.npmjs.org/install.sh | sh

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

## What is a `module`?

A module is anything that can be loaded with `require()` in a Node.js
program.  The following things are all examples of things that can be
loaded as modules:

* A folder with a `package.json` file containing a `main` field.
* A folder with an `index.js` file in it.
* A JavaScript file.

Most npm packages are modules, because they are libraries that you
load with `require`.  However, there's no requirement that an npm
package be a module!  Some only contain an executable command-line
interface, and don't provide a `main` field for use in Node programs.

Almost all npm packages (at least, those that are Node programs)
*contain* many modules within them (because every file they load with
`require()` is a module).

In the context of a Node program, the `module` is also the thing that
was loaded *from* a file.  For example, in the following program:

    var req = require('request')

we might say that "The variable `req` refers to the `request` module".

## So, why is it the "`node_modules`" folder, but "`package.json`" file?  Why not `node_packages` or `module.json`?

The `package.json` file defines the package.  (See "What is a
package?" above.)

The `node_modules` folder is the place Node.js looks for modules.
(See "What is a module?" above.)

For example, if you create a file at `node_modules/foo.js` and then
had a program that did `var f = require('foo.js')` then it would load
the module.  However, `foo.js` is not a "package" in this case,
because it does not have a package.json.

Alternatively, if you create a package which does not have an
`index.js` or a `"main"` field in the `package.json` file, then it is
not a module.  Even if it's installed in `node_modules`, it can't be
an argument to `require()`.

## `"node_modules"` is the name of my deity's arch-rival, and a Forbidden Word in my religion.  Can I configure npm to use a different folder?

No.  This will never happen.  This question comes up sometimes,
because it seems silly from the outside that npm couldn't just be
configured to put stuff somewhere else, and then npm could load them
from there.  It's an arbitrary spelling choice, right?  What's the big
deal?

At the time of this writing, the string `'node_modules'` appears 151
times in 53 separate files in npm and node core (excluding tests and
documentation).

Some of these references are in node's built-in module loader.  Since
npm is not involved **at all** at run-time, node itself would have to
be configured to know where you've decided to stick stuff.  Complexity
hurdle #1.  Since the Node module system is locked, this cannot be
changed, and is enough to kill this request.  But I'll continue, in
deference to your deity's delicate feelings regarding spelling.

Many of the others are in dependencies that npm uses, which are not
necessarily tightly coupled to npm (in the sense that they do not read
npm's configuration files, etc.)  Each of these would have to be
configured to take the name of the `node_modules` folder as a
parameter.  Complexity hurdle #2.

Furthermore, npm has the ability to "bundle" dependencies by adding
the dep names to the `"bundledDependencies"` list in package.json,
which causes the folder to be included in the package tarball.  What
if the author of a module bundles its dependencies, and they use a
different spelling for `node_modules`?  npm would have to rename the
folder at publish time, and then be smart enough to unpack it using
your locally configured name.  Complexity hurdle #3.

Furthermore, what happens when you *change* this name?  Fine, it's
easy enough the first time, just rename the `node_modules` folders to
`./blergyblerp/` or whatever name you choose.  But what about when you
change it again?  npm doesn't currently track any state about past
configuration settings, so this would be rather difficult to do
properly.  It would have to track every previous value for this
config, and always accept any of them, or else yesterday's install may
be broken tomorrow.  Complexity hurdle #4.

Never going to happen.  The folder is named `node_modules`.  It is
written indelibly in the Node Way, handed down from the ancient times
of Node 0.3.

## How do I install node with npm?

You don't.  Try one of these node version managers:

Unix:

* <http://github.com/isaacs/nave>
* <http://github.com/visionmedia/n>
* <http://github.com/creationix/nvm>

Windows:

* <http://github.com/marcelklehr/nodist>
* <https://github.com/hakobera/nvmw>
* <https://github.com/nanjingboy/nvmw>

## How can I use npm for development?

See `npm-developers(7)` and `package.json(5)`.

You'll most likely want to `npm link` your development folder.  That's
awesomely handy.

To set up your own private registry, check out `npm-registry(7)`.

## Can I list a url as a dependency?

Yes.  It should be a url to a gzipped tarball containing a single folder
that has a package.json in its root, or a git url.
(See "what is a package?" above.)

## How do I symlink to a dev folder so I don't have to keep re-installing?

See `npm-link(1)`

## The package registry website.  What is that exactly?

See `npm-registry(7)`.

## I forgot my password, and can't publish.  How do I reset it?

Go to <https://npmjs.org/forgot>.

## I get ECONNREFUSED a lot.  What's up?

Either the registry is down, or node's DNS isn't able to reach out.

To check if the registry is down, open up
<https://registry.npmjs.org/> in a web browser.  This will also tell
you if you are just unable to access the internet for some reason.

If the registry IS down, let us know by emailing <support@npmjs.com>
or posting an issue at <https://github.com/npm/npm/issues>.  If it's
down for the world (and not just on your local network) then we're
probably already being pinged about it.

You can also often get a faster response by visiting the #npm channel
on Freenode IRC.

## Why no namespaces?

Please see this discussion: <https://github.com/npm/npm/issues/798>

tl;dr - It doesn't actually make things better, and can make them worse.

If you want to namespace your own packages, you may: simply use the
`-` character to separate the names.  npm is a mostly anarchic system.
There is not sufficient need to impose namespace rules on everyone.

## Who does npm?

npm was originally written by Isaac Z. Schlueter, and many others have
contributed to it, some of them quite substantially.

The npm open source project, The npm Registry, and [the community
website](https://www.npmjs.org) are maintained and operated by the
good folks at [npm, Inc.](http://www.npmjs.com)

## I have a question or request not addressed here. Where should I put it?

Post an issue on the github project:

* <https://github.com/npm/npm/issues>

## Why does npm hate me?

npm is not capable of hatred.  It loves everyone, especially you.

## SEE ALSO

* npm(1)
* npm-developers(7)
* package.json(5)
* npm-config(1)
* npm-config(7)
* npmrc(5)
* npm-config(7)
* npm-folders(5)
