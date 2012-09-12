deputy
======

This module is a caching layer around
[node-detective](http://github.com/substack/node-detective).

[![build status](https://secure.travis-ci.org/substack/node-deputy.png)](http://travis-ci.org/substack/node-deputy)

example
=======

cache.js
--------

``` js
var deputy = require('deputy');
var detective = deputy(process.env.HOME + '/.config/deputy.json');

var deps = detective.find('require("a"); require("b")');
console.dir(deps);
```

output:

```
$ node cache.js 
{ strings: [ 'a', 'b' ], expressions: [] }
$ cat ~/.config/deputy.json 
{"55952d490bd28e3e256f0b036ced834d":{"strings":["a","b"],"expressions":[]}}
```

methods
=======

``` js
var deputy = require('deputy')
```

var detective = deputy(cacheFile)
---------------------------------

Return a new [detective](http://github.com/substack/node-detective)
object using `cacheFile` for caching.

install
=======

With [npm](http://npmjs.org) do:

    npm install deputy

license
=======

MIT/X11
