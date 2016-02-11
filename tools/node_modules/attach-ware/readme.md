# attach-ware [![Build Status](https://img.shields.io/travis/wooorm/attach-ware.svg)](https://travis-ci.org/wooorm/attach-ware) [![Coverage Status](https://img.shields.io/codecov/c/github/wooorm/attach-ware.svg)](https://codecov.io/github/wooorm/attach-ware)

Middleware with configuration.

## Installation

[npm](https://docs.npmjs.com/cli/install):

```bash
npm install attach-ware
```

**attach-ware** is also available for [bower](http://bower.io/#install-packages),
[component](https://github.com/componentjs/component), and [duo](http://duojs.org/#getting-started),
and as an AMD, CommonJS, and globals module, [uncompressed](attach-ware.js) and
[compressed](attach-ware.min.js).

## Usage

`x.js`:

```js
module.exports = function (ctx, options) {
    if (!options.condition) return;

    return function (req, res, next) {
        res.x = 'hello';
        next();
    };
}
```

`y.js`:

```js
module.exports = function (ctx, options) {
    if (!options.condition) return;

    return function (req, res, next) {
        res.y = 'world';
        next();
    };
}
```

`index.js`:

```js
var ware = require('attach-ware')(require('ware'));
var x = require('./x.js');
var y = require('./y.js');

var middleware = attachWare()
    .use(x, {'condition': true})
    .use(y, {'condition': false})
    .run({}, {}, function (err, req, res) {
        console.log(res.x); // "hello"
        console.log(res.y); // undefined
    });
```

## API

> **Note**: first create a new constructor (later called `AttachWare`)
> by passing `ware` to the by this module exposed function. This
> enables ware-like libraries to be used too.

### new AttachWare()

Create configurable middleware. Works just like [`ware()`](https://github.com/segmentio/ware#ware-1).

### AttachWare#use(attach, input...)

Invokes `attach` with either `attachWare.context` or `attachWare`,
and all `input`.

If `attach` returns another function (`fn`, which can be synchronous,
asynchronous, or a generator function), that function is [added to the
middleware](https://github.com/segmentio/ware#usefn), and will be invoked when
[`run()`](https://github.com/segmentio/ware#runinput-callback) is invoked
like normal middleware.

### AttachWare#context

The first argument for `attach`ers. When this is falsey, `attachWare`
itself is used.

## License

[MIT](LICENSE) © [Titus Wormer](http://wooorm.com)
