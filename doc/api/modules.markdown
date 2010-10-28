## Standard Modules

Node comes with a number of modules that are compiled in to the process,
most of which are documented below.  The most common way to use these modules
is with `require('name')` and then assigning the return value to a local
variable with the same name as the module.

Example:

    var util = require('util');
    
It is possible to extend node with other modules.  See `'Modules'`

## Modules

Node uses the CommonJS module system.

Node has a simple module loading system.  In Node, files and modules are in
one-to-one correspondence.  As an example, `foo.js` loads the module
`circle.js` in the same directory.

The contents of `foo.js`:

    var circle = require('./circle');
    console.log( 'The area of a circle of radius 4 is '
               + circle.area(4));

The contents of `circle.js`:

    var PI = 3.14;

    exports.area = function (r) {
      return PI * r * r;
    };

    exports.circumference = function (r) {
      return 2 * PI * r;
    };

The module `circle.js` has exported the functions `area()` and
`circumference()`.  To export an object, add to the special `exports`
object.  (Alternatively, one can use `this` instead of `exports`.) Variables
local to the module will be private. In this example the variable `PI` is
private to `circle.js`. The function `puts()` comes from the module `'util'`,
which is a built-in module. Modules which are not prefixed by `'./'` are
built-in module--more about this later.

### Module Resolving

A module prefixed with `'./'` is relative to the file calling `require()`.
That is, `circle.js` must be in the same directory as `foo.js` for
`require('./circle')` to find it.

Without the leading `'./'`, like `require('assert')` the module is searched
for in the `require.paths` array. `require.paths` on my system looks like
this: 

`[ '/home/ryan/.node_modules' ]`

That is, when `require('foo')` is called Node looks for:

* 1: `/home/ryan/.node_modules/foo`
* 2: `/home/ryan/.node_modules/foo.js`
* 3: `/home/ryan/.node_modules/foo.node`
* 4: `/home/ryan/.node_modules/foo/index.js`
* 5: `/home/ryan/.node_modules/foo/index.node`

interrupting once a file is found. Files ending in `'.node'` are binary Addon
Modules; see 'Addons' below. `'index.js'` allows one to package a module as
a directory.

`require.paths` can be modified at runtime by simply unshifting new
paths onto it, or at startup with the `NODE_PATH` environmental
variable (which should be a list of paths, colon separated).
Additionally node will search for directories called `node_modules` starting
at the current directory (of the module calling `require`) and upwards
towards the root of the package tree.
This feature makes it easy to have different module versions for different
environments. Imagine the situation where you have a devopment environment
and a production environment each with a different version of the `foo`
module: `projects/x/development/node_modules/foo` and
`projects/x/production/node_modules/foo`.


The second time `require('foo')` is called, it is not loaded again from
disk. It looks in the `require.cache` object to see if it has been loaded
before.

To get the exact filename that will be loaded when `require()` is called, use
the `require.resolve()` function.
