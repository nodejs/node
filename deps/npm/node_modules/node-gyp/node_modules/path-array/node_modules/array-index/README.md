array-index
===========
### Invoke getter/setter functions on array-like objects
[![Build Status](https://secure.travis-ci.org/TooTallNate/array-index.svg)](http://travis-ci.org/TooTallNate/array-index)


This little module provides an `ArrayIndex` constructor function that you can
inherit from with your own objects. When a numbered property gets read, then the
`__get__` function on the object will be invoked. When a numbered property gets
set, then the `__set__` function on the object will be invoked.


Installation
------------

Install with `npm`:

``` bash
$ npm install array-index
```


Examples
--------

A quick silly example, using `Math.sqrt()` for the "getter":

``` js
var ArrayIndex = require('array-index')

// let's just create a singleton instance.
var a = new ArrayIndex()

// the "__get__" function is invoked for each "a[n]" access.
// it is given a single argument, the "index" currently being accessed.
// so here, we're passing in the `Math.sqrt()` function, so accessing
// "a[9]" will return `Math.sqrt(9)`.
a.__get__ = Math.sqrt

// the "__get__" and "__set__" functions are only invoked up
// to "a.length", so we must set that manually.
a.length = 10

console.log(a)
// [ 0,
//   1,
//   1.4142135623730951,
//   1.7320508075688772,
//   2,
//   2.23606797749979,
//   2.449489742783178,
//   2.6457513110645907,
//   2.8284271247461903,
//   3,
//   __get__: [Function: sqrt] ]
```

Here's an example of creating a subclass of `ArrayIndex` using `util.inherits()`:

``` js
var ArrayIndex = require('array-index')
var inherits = require('util').inherits

function MyArray (length) {
  // be sure to call the ArrayIndex constructor in your own constructor
  ArrayIndex.call(this, length)

  // the "set" object will contain values at indexes previously set,
  // so that they can be returned in the "getter" function. This is just a
  // silly example, your subclass will have more meaningful logic.
  Object.defineProperty(this, 'set', {
    value: Object.create(null),
    enumerable: false
  })
}

// inherit from the ArrayIndex's prototype
inherits(MyArray, ArrayIndex)

MyArray.prototype.__get__ = function (index) {
  if (index in this.set) return this.set[index]
  return index * 2
}

MyArray.prototype.__set__ = function (index, v) {
  this.set[index] = v
}


// and now you can create some instances
var a = new MyArray(15)
a[9] = a[10] = a[14] = '_'
a[0] = 'nate'

console.log(a)
// [ 'nate', 2, 4, 6, 8, 10, 12, 14, 16, '_', '_', 22, 24, 26, '_' ]
```

API
---

The `ArrayIndex` base class is meant to be subclassed, but it also has a few
convenient functions built-in.

### "length" -> Number

The length of the ArrayIndex instance. The `__get__` and `__set__` functions will
only be invoked on the object up to this "length". You may set this length at any
time to adjust the amount range where the getters/setters will be invoked.

### "toArray()" -> Array

Returns a new regular Array instance with the same values that this ArrayIndex
class would have. This function calls the `__get__` function repeatedly from
`0...length-1` and returns the "flattened" array instance.

### "toJSON()" -> Array

All `ArrayIndex` instances get basic support for `JSON.stringify()`, which is
the same as a "flattened" Array being stringified.

### "toString()" -> String

The `toString()` override is basically just `array.toArray().toString()`.

### "format()" -> String

The `inspect()` implementation for the REPL attempts to mimic what a regular
Array looks like in the REPL.


License
-------

(The MIT License)

Copyright (c) 2012 Nathan Rajlich &lt;nathan@tootallnate.net&gt;

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
