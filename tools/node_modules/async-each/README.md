# async-each

No-bullshit, ultra-simple, 35-lines-of-code async parallel forEach function for JavaScript.

We don't need junky 30K async libs. Really.

For browsers and node.js.

## Installation
* Just include async-each before your scripts.
* `npm install async-each` if you’re using node.js.
* `component install paulmillr/async-each` if you’re using [component(1)](https://github.com/component/component).
* `bower install async-each` if you’re using [Twitter Bower](http://bower.io).

## Usage

* `each(array, iterator, callback);` — `Array`, `Function`, `(optional) Function`
* `iterator(item, next)` receives current item and a callback that will mark the item as done. `next` callback receives optional `error, transformedItem` arguments.
* `callback(error, transformedArray)` optionally receives first error and transformed result `Array`.

Node.js:

```javascript
var each = require('async-each');
each(['a.js', 'b.js', 'c.js'], fs.readFile, function(error, contents) {
  if (error) console.error(error);
  console.log('Contents for a, b and c:', contents);
});
```

Browser:

```javascript
// component(1)
var each = require('async-each');
each(list, fn, callback);

// Default:
window.asyncEach(list, fn, callback);
```

## License

The MIT License (MIT)

Copyright (c) 2013 Paul Miller (http://paulmillr.com/)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the “Software”), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
