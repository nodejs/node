methods
=======

This section documents the browserify api.

````javascript
var browserify = require('browserify');
````

var b = browserify(opts={})
---------------------------

Return a new bundle object.

`opts` may also contain these fields:

* watch - set watches on files, see below
* cache - turn on caching for AST traversals, see below
* debug - turn on source mapping for debugging with `//@ sourceURL=...`
in browsers that support it
* exports - an array of the core items to export to the namespace. Available
items: 'require', 'process'

If `opts` is a string, it is interpreted as a file to call `.addEntry()` with.

### watch :: Boolean or Object

Set watches on files and automatically rebundle when a file changes.

This option defaults to false. If `opts.watch` is set to true, default watch
arguments are assumed or you can pass in an object to pass along as the second
parameter to `fs.watchFile()`.

### cache :: Boolean or String

If `cache` is a boolean, turn on caching at
`$HOME/.config/browserify/cache.json`.

If `cache` is a string, turn on caching at the filename specified by `cache`.

### bundle events

`b` bundles will also emit events.

#### 'syntaxError', err

This event gets emitted when there is a syntax error somewhere in the build
process. If you don't listen for this event, the error will be printed to
stderr.

#### 'bundle'

In watch mode, this event is emitted when a new bundle has been generated.

b.bundle()
----------

Return the bundled source as a string.

By default, `require` is not exported to the environment if there are entry
files in the bundle but you can override that with `opts.exports`.

`process` is only exported to the environment when `opts.exports` contains the
string `'process'`.

b.require(file)
---------------

Require a file or files for inclusion in the bundle.

If `file` is an array, require each element in it.

If `file` is a non-array object, map an alias to a package name.
For instance to be able to map `require('jquery')` to the jquery-browserify
package, you can do:

````javascript
b.require({ jquery : 'jquery-browserify' })
````

and the same thing in middleware-form:

````javascript
browserify({ require : { jquery : 'jquery-browserify' } })
````

To mix alias objects with regular requires you could do:

````javascript
browserify({ require : [ 'seq', { jquery : 'jquery-browserify' }, 'traverse' ])
````

In practice you won't need to `b.require()` very many files since all the
`require()`s are read from each file that you require and automatically
included.

b.ignore(file)
--------------

Omit a file or files from being included by the AST walk to hunt down
`require()` statements.

b.addEntry(file)
----------------

Append a file to the end of the bundle and execute it without having to
`require()` it.

Specifying an entry point will let you `require()` other modules without having
to load the entry point in a `<script>` tag yourself.

If entry is an Array, concatenate these files together and append to the end of
the bundle.

b.filter(fn)
------------

Transform the source using the filter function `fn(src)`. The return value of
`fn` should be the new source.

b.register(ext, fn)
-------------------

Register a handler to wrap extensions.

Wrap every file matching the extension `ext` with the function `fn`.

For every `file` included into the bundle `fn` gets called for matching file
types as `fn.call(b, body, file)` for the bundle instance `b` and the file
content string `body`. `fn` should return the new wrapped contents.

If `ext` is unspecified, execute the wrapper for every file.

If `ext` is 'post', execute the wrapper on the entire bundle.

If `ext` is 'pre', call the wrapper function with the bundle object before the
source is generated.

If `ext` is an object, pull the extension from `ext.extension` and the wrapper
function `fn` from `ext.wrapper`. This makes it easy to write plugins like
[fileify](https://github.com/substack/node-fileify).

Coffee script support is just implemented internally as a `.register()`
extension:

````javascript
b.register('.coffee', function (body) {
    return coffee.compile(body);
});
````

b.use(fn)
---------

Use a middleware plugin, `fn`. `fn` is called with the instance object `b`.

b.prepend(content)
------------------

Prepend unwrapped content to the beginning of the bundle.

b.append(content)
-----------------

Append unwrapped content to the end of the bundle.

b.alias(to, from)
-----------------

Alias a package name from another package name.

b.modified
----------

Contains a Date object with the time the bundle was last modified. This field is
useful in conjunction with the `watch` field described in the `browserify()` to
generate unique `<script>` `src` values to force script reloading.
