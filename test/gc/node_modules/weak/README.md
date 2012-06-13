node-weak
=========
### Make weak references to JavaScript Objects.
[![Build Status](https://secure.travis-ci.org/TooTallNate/node-weak.png)](http://travis-ci.org/TooTallNate/node-weak)

On certain rarer occasions, you run into the need to be notified when a JavaScript
object is going to be garbage collected. This feature is exposed to V8's C++ API,
but not to JavaScript.

That's where `node-weak` comes in! This module exports V8's `Persistent<Object>`
functionality to JavaScript. This allows you to create weak references, and
optionally attach a callback function to any arbitrary JS object. The callback
function will be invoked right before the Object is garbage collected (i.e. after
there are no more remaining references to the Object in JS-land).

This module can, for example, be used for debugging; to determine whether or not
an Object is being garbage collected as it should.
Take a look at the example below for commented walkthrough scenario.


Installation
------------

Install with `npm`:

``` bash
$ npm install weak
```


Example
-------

Here's an example of calling a `cleanup()` function on a Object before it gets
garbage collected:

``` js
var weak = require('weak')

// we are going to "monitor" this Object and invoke "cleanup"
// before the object is garbage collected
var obj = {
    a: true
  , foo: 'bar'
}

// The function to call before Garbage Collection.
// Note that by the time this is called, 'obj' has been set to `null`.
function cleanup (o) {
  delete o.a
  delete o.foo
}

// Here's where we set up the weak reference
var ref = weak(obj, function () {
  // `this` inside the callback is the 'obj'. DO NOT store any new references
  // to the object, and DO NOT use the object in any async functions.
  cleanup(this)
})

// While `obj` is alive, `ref` proxies everything to it, so:
ref.a   === obj.a
ref.foo === obj.foo

// Clear out any references to the object, so that it will be GC'd at some point...
obj = null

//
//// Time passes, and the garbage collector is run
//

// `callback()` above is called, and `ref` now acts like an empty object.
typeof ref.foo === 'undefined'
```


API
---

### weakref weak(Object obj [, Function callback])

The main exports is the function that creates the weak reference.
The first argument is the Object that should be monitored.
The Object can be a regular Object, an Array, a Function, a RegExp, or any of
the primitive types or constructor function created with `new`.
Optionally, you can set a callback function to be invoked
before the object is garbage collected.


### Object weak.get(weakref ref)

`get()` returns the actual reference to the Object that this weak reference was
created with. If this is called with a dead reference, `undefined` is returned.


### Boolean weak.isDead(weakref ref)

Checks to see if `ref` is a dead reference. Returns `true` if the original Object
has already been GC'd, `false` otherwise.


### null weak.addCallback(weakref ref, Function callback)

Adds `callback` to the Array of callback functions that will be invoked before the
Objects gets garbage collected. The callbacks get executed in the order that they
are added.


### Array weak.callbacks(weakref ref)

Returns the internal `Array` that `ref` iterates through to invoke the GC
callbacks. The array can be `push()`ed or `unshift()`ed onto, to have more control
over the execution order of the callbacks. This is similar in concept to node's
`EventEmitter#listeners()` function.
