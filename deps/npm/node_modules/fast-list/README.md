# The Problem

You've got some thing where you need to push a bunch of stuff into a
queue and then shift it out.  Or, maybe it's a stack, and you're just
pushing and popping it.

Arrays work for this, but are a bit costly performance-wise.

# The Solution

A linked-list implementation that takes advantage of what v8 is good at:
creating objects with a known shape.

This is faster for this use case.  How much faster?  About 50%.

    $ node bench.js
    benchmarking /Users/isaacs/dev-src/js/fast-list/bench.js
    Please be patient.
    { node: '0.6.2-pre',
      v8: '3.6.6.8',
      ares: '1.7.5-DEV',
      uv: '0.1',
      openssl: '0.9.8l' }
    Scores: (bigger is better)

    new FastList()
    Raw:
     > 22556.39097744361
     > 23054.755043227666
     > 22770.398481973436
     > 23414.634146341465
     > 23099.133782483157
    Average (mean) 22979.062486293868

    []
    Raw:
     > 12195.121951219513
     > 12184.508268059182
     > 12173.91304347826
     > 12216.404886561955
     > 12184.508268059182
    Average (mean) 12190.891283475617

    new Array()
    Raw:
     > 12131.715771230503
     > 12184.508268059182
     > 12216.404886561955
     > 12195.121951219513
     > 11940.298507462687
    Average (mean) 12133.609876906768

    Winner: new FastList()
    Compared with next highest ([]), it's:
    46.95% faster
    1.88 times as fast
    0.28 order(s) of magnitude faster

    Compared with the slowest (new Array()), it's:
    47.2% faster
    1.89 times as fast
    0.28 order(s) of magnitude faster

This lacks a lot of features that arrays have:

1. You can't specify the size at the outset.
2. It's not indexable.
3. There's no join, concat, etc.

If any of this matters for your use case, you're probably better off
using an Array object.

## Installing

```
npm install fast-list
```

## API

```javascript
var FastList = require("fast-list")
var list = new FastList()
list.push("foo")
list.unshift("bar")
list.push("baz")
console.log(list.length) // 2
console.log(list.pop()) // baz
console.log(list.shift()) // bar
console.log(list.shift()) // foo
```

### Methods

* `push`: Just like Array.push, but only can take a single entry
* `pop`: Just like Array.pop
* `shift`: Just like Array.shift
* `unshift`: Just like Array.unshift, but only can take a single entry
* `drop`: Drop all entries
* `item(n)`: Retrieve the nth item in the list.  This involves a walk
  every time.  It's very slow.  If you find yourself using this,
  consider using a normal Array instead.
* `map(fn, thisp)`: Like `Array.prototype.map`.  Returns a new FastList.
* `reduce(fn, startValue, thisp)`: Like `Array.prototype.reduce`
* `forEach(fn, this)`: Like `Array.prototype.forEach`
* `filter(fn, thisp)`: Like `Array.prototype.filter`. Returns a new
  FastList.
* `slice(start, end)`: Retrieve an array of the items at this position.
  This involves a walk every time.  It's very slow.  If you find
  yourself using this, consider using a normal Array instead.

### Members

* `length`: The number of things in the list.  Note that, unlike
  Array.length, this is not a getter/setter, but rather a counter that
  is internally managed.  Setting it can only cause harm.
