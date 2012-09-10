detective
=========

Find all calls to require() no matter how crazily nested using a proper walk of
the AST.

example
=======

strings
-------

strings_src.js:

````javascript
var a = require('a');
var b = require('b');
var c = require('c');
````

strings.js:

````javascript
var detective = require('detective');
var fs = require('fs');

var src = fs.readFileSync(__dirname + '/strings_src.js');
var requires = detective(src);
console.dir(requires);
````

output:

    $ node examples/strings.js
    [ 'a', 'b', 'c' ]

methods
=======

var detective = require('detective');

detective(src, opts)
--------------------

Give some source body `src`, return an array of all the require()s with string
arguments.

The options parameter `opts` is passed along to `detective.find()`.

detective.find(src, opts)
-------------------------

Give some source body `src`, return an object with "strings" and "expressions"
arrays for each of the require() calls.

The "expressions" array will contain the stringified expressions.

Optionally you can specify a different function besides `"require"` to analyze
with `opts.word`.

installation
============

    npm install detective
