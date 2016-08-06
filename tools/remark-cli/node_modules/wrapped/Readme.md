
# wrapped

  Low-level wrapper to provide a consistent interface for sync, async, promises, and generator functions.

  Works with node.js and in the browser.

## Installation

Node:

```bash
$ npm install wrapped
```

Duo:

```js
var wrap = require('matthewmueller/wrapped');
```

## Example

```js
function *fn(a, b) {
  yield wait(100);
  // ...
}

function next(err) {
  // Called after
}

var wrap = wrapped(fn)
wrap('a', 'b', next)
```

## API

### `wrap(fn)([args, ...], [done])`

Wrap `fn` to support sync, async, promises and generator functions. Call `done` when finished.

`wrap` returns a function which you can pass arguments to or set the context.

```js
wrap(fn).call(user, a, b, c, d, next);
```

## Test

```js
npm install
make test
```

## License

(The MIT License)

Copyright (c) 2014 Matthew Mueller &lt;mattmuelle@gmail.com&gt;

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
