browserify
==========

Make node-style require() work in the browser with a server-side build step,
as if by magic!

[![build status](https://secure.travis-ci.org/substack/node-browserify.png)](http://travis-ci.org/substack/node-browserify)

![browserify!](http://substack.net/images/browserify/browserify.png)

example
=======

Just write an `entry.js` to start with some `require()`s in it:

````javascript
// use relative requires
var foo = require('./foo');
var bar = require('../lib/bar');

// or use modules installed by npm in node_modules/
var domready = require('domready');

domready(function () {
    var elem = document.getElementById('result');
    elem.textContent = foo(100) + bar('baz');
});
````

Now just use the `browserify` command to build a bundle starting at `entry.js`:

```
$ browserify entry.js -o bundle.js
```

All of the modules that `entry.js` needs are included in the final bundle from a
recursive walk using [detective](https://github.com/substack/node-detective).

To use the bundle, just toss a `<script src="bundle.js"></script>` into your
html!

usage
=====

````
Usage: browserify [entry files] {OPTIONS}

Options:
  --outfile, -o  Write the browserify bundle to this file.
                 If unspecified, browserify prints to stdout.                   
  --require, -r  A module name or file to bundle.require()
                 Optionally use a colon separator to set the target.            
  --entry, -e    An entry point of your app                                     
  --exports      Export these core objects, comma-separated list
                 with any of: require, process. If unspecified, the
                 export behavior will be inferred.
                                                                                
  --ignore, -i   Ignore a file                                                  
  --alias, -a    Register an alias with a colon separator: "to:from"
                 Example: --alias 'jquery:jquery-browserify'                    
  --cache, -c    Turn on caching at $HOME/.config/browserling/cache.json or use
                 a file for caching.
                                                                 [default: true]
  --debug, -d    Switch on debugging mode with //@ sourceURL=...s.     [boolean]
  --plugin, -p   Use a plugin.
                 Example: --plugin aliasify                                     
  --prelude      Include the code that defines require() in this bundle.
                                                      [boolean]  [default: true]
  --watch, -w    Watch for changes. The script will stay open and write updates
                 to the output every time any of the bundled files change.
                 This option only works in tandem with -o.                      
  --verbose, -v  Write out how many bytes were written in -o mode. This is
                 especially useful with --watch.                                
  --help, -h     Show this message                                              

````

compatability
=============

Many [npm](http://npmjs.org) modules that don't do IO will just work after being
browserified. Others take more work.

[coffee script](http://coffeescript.org/) should pretty much just work.
Just do `browserify entry.coffee` or `require('./foo.coffee')`.

Many node built-in modules have been wrapped to work in the browser.
All you need to do is `require()` them like in node.

* events
* path
* [vm](https://github.com/substack/vm-browserify)
* [http](https://github.com/substack/http-browserify)
* [crypto](https://github.com/dominictarr/crypto-browserify)
* assert
* url
* buffer
* buffer_ieee754
* util
* querystring
* stream

process
-------

Browserify makes available a faux `process` object to modules with these
attributes:

* nextTick(fn) - uses [the postMessage trick](http://dbaron.org/log/20100309-faster-timeouts)
    for a faster `setTimeout(fn, 0)` if it can
* title - set to 'browser' for browser code, 'node' in regular node code
* browser - `true`, good for testing if you're in a browser or in node

By default the process object is only available inside of files wrapped by
browserify. To expose it, use `--exports=process`

__dirname
---------

The faux directory name, scrubbed of true directory information so as not to
expose your filesystem organization.

__filename
----------

The faux file path, scrubbed of true path information so as not to expose your
filesystem organization.

package.json
============

In order to resolve main files for projects, the package.json "main" field is
read.

If a package.json has a "browserify" field, you can override the standard "main"
behavior with something special just for browsers.

See [dnode's
package.json](https://github.com/substack/dnode/blob/9e24b97cf2ce931fbf6d7beb3731086b46bca887/package.json#L40)
for an example of using the "browserify" field.

more
====

* [browserify recipes](https://github.com/substack/node-browserify/blob/master/doc/recipes.markdown#recipes)
* [browserify api reference](https://github.com/substack/node-browserify/blob/master/doc/methods.markdown#methods)
* [browserify cdn](http://browserify.nodejitsu.com/)

install
=======

With [npm](http://npmjs.org) do:

```
npm install -g browserify
```

test
====

To run the node tests with tap, do:

```
npm test
```

To run the [testling](http://testling.com) tests,
create a [browserling](http://browserling.com) account then:

```
cd testling
./test.sh
```
