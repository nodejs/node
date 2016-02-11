
# ware

  Easily create your own middleware layer.

  [![Build Status](https://travis-ci.org/segmentio/ware.png?branch=master)](https://travis-ci.org/segmentio/ware)

## Installation

Node:

```bash
$ npm install ware
```

Component:

```bash
$ component install segmentio/ware
```

Duo:

```js
var ware = require('segmentio/ware');
```

## Example

```js
var ware = require('ware');
var middleware = ware()
  .use(function (req, res, next) {
    res.x = 'hello';
    next();
  })
  .use(function (req, res, next) {
    res.y = 'world';
    next();
  });

middleware.run({}, {}, function (err, req, res) {
  res.x; // "hello"
  res.y; // "world"
});
```

  Give it any number of arguments:

```js
var ware = require('ware');
var middleware = ware()
  .use(function (a, b, c, next) {
    console.log(a, b, c);
    next();
  })

middleware.run(1, 2, 3); // 1, 2, 3
```

  Supports generators (on the server):

```js
var ware = require('ware');
var middleware = ware()
  .use(function (obj) {
    obj.url = 'http://google.com';
  })
  .use(function *(obj) {
    obj.src = yield http.get(obj.url);
  })

middleware.run({ url: 'http://facebook.com' }, function(err, obj) {
  if (err) throw err;
  obj.src // "obj.url" source
});
```

## API

#### ware()

  Create a new list of middleware.

#### .use(fn)

  Push a middleware `fn` onto the list. `fn` can be a synchronous, asynchronous, or generator function.
  `fn` can also be an array of functions or an instance of `Ware`.

#### .run(input..., [callback])

  Runs the middleware functions with `input...` and optionally calls `callback(err, input...)`.

## License

  (The MIT License)

  Copyright (c) 2013 Segment.io &lt;friends@segment.io&gt;

  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the 'Software'), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
