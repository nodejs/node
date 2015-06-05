path-array
==========
### Treat your `$PATH` like a JavaScript Array
[![Build Status](https://travis-ci.org/TooTallNate/path-array.svg?branch=master)](https://travis-ci.org/TooTallNate/path-array)

This module provides a JavaScript `Array` implementation that is backed by your
`$PATH` env variable. That is, you can use regular Array functions like `shift()`,
`pop()`, `push()`, `unshift()`, etc. to mutate your `$PATH`.

Also works for preparing an `env` object for passing to
[`child_process.spawn()`][cp.spawn].


Installation
------------

Install with `npm`:

``` bash
$ npm install path-array
```


Example
-------

Interacting with your own `$PATH` env variable:

``` js
var PathArray = require('path-array');

// no args uses `process.env` by default
var p = new PathArray();

console.log(p);
// [ './node_modules/.bin',
//   '/opt/local/bin',
//   '/opt/local/sbin',
//   '/usr/local/bin',
//   '/usr/local/sbin',
//   '/usr/bin',
//   '/bin',
//   '/usr/sbin',
//   '/sbin',
//   '/usr/local/bin',
//   '/opt/X11/bin' ]

// push another path entry. this function mutates the `process.env.PATH`
p.unshift('/foo');

console.log(process.env.PATH);
// '/foo:./node_modules/.bin:/opt/local/bin:/opt/local/sbin:/usr/local/bin:/usr/local/sbin:/usr/bin:/bin:/usr/sbin:/sbin:/usr/local/bin:/opt/X11/bin'
```


API
---

### new PathArray([env]) → PathArray

Creates and returns a new `PathArray` instance with the given `env` object. If no
`env` is specified, then [`process.env`][process.env] is used by default.


License
-------

(The MIT License)

Copyright (c) 2013 Nathan Rajlich &lt;nathan@tootallnate.net&gt;

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
'Software'), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

[process.env]: http://nodejs.org/docs/latest/api/process.html#process_process_env
[cp.spawn]: http://nodejs.org/docs/latest/api/child_process.html#child_process_child_process_spawn_command_args_options
