# HAR Validator [![version][npm-version]][npm-url] [![License][npm-license]][license-url]

Extremely fast HTTP Archive ([HAR](http://www.softwareishard.com/blog/har-12-spec/)) validator using JSON Schema.

[![Build Status][travis-image]][travis-url]
[![Downloads][npm-downloads]][npm-url]
[![Code Climate][codeclimate-quality]][codeclimate-url]
[![Coverage Status][codeclimate-coverage]][codeclimate-url]
[![Dependencies][david-image]][david-url]

## Install

```shell
# to use in cli
npm install --global har-validator

# to use as a module
npm install --save har-validator
```

## Usage

```

  Usage: har-validator [options] <files ...>

  Options:

    -h, --help           output usage information
    -V, --version        output the version number
    -s, --schema [name]  validate schema name (log, request, response, etc ...)

```

###### Example

```shell
har-validator har.json

har-validator --schema request request.json
```

## API

**Note**: as of [`v2.0.0`](https://github.com/ahmadnassri/har-validator/releases/tag/v2.0.0) this module defaults to Promise based API. *For backward comptability with `v1.x` an [async/callback API](#callback-api) is provided*

### Validate(data)

> Returns a promise that resolves to the valid object.

- **data**: `Object` *(Required)*
  a full [HAR](http://www.softwareishard.com/blog/har-12-spec/) object

```js
validate(data)
  .then(data => console.log('horray!'))
  .catch(console.error)
```

### Validate.log(data)

> Returns a promise that resolves to the valid object.

- **data**: `Object` *(Required)*
  a [log](http://www.softwareishard.com/blog/har-12-spec/#log) object

```js
validate.log(data)
  .then(data => console.log('horray!'))
  .catch(console.error)
```

### Validate.cache(data)

> Returns a promise that resolves to the valid object.

- **data**: `Object` *(Required)*
  a [cache](http://www.softwareishard.com/blog/har-12-spec/#cache) object

```js
validate.cache(data)
  .then(data => console.log('horray!'))
  .catch(console.error)
```

### Validate.cacheEntry(data)

> Returns a promise that resolves to the valid object.

- **data**: `Object` *(Required)*
  a ["beforeRequest" or "afterRequest"](http://www.softwareishard.com/blog/har-12-spec/#cache) objects

```js
validate.cacheEntry(data)
  .then(data => console.log('horray!'))
  .catch(console.error)
```

### Validate.content(data)

> Returns a promise that resolves to the valid object.

- **data**: `Object` *(Required)*
  a [content](http://www.softwareishard.com/blog/har-12-spec/#content) object

```js
validate.content(data)
  .then(data => console.log('horray!'))
  .catch(console.error)
```

### Validate.cookie(data)

> Returns a promise that resolves to the valid object.

- **data**: `Object` *(Required)*
  a [cookie](http://www.softwareishard.com/blog/har-12-spec/#cookies) object

```js
validate.cookie(data)
  .then(data => console.log('horray!'))
  .catch(console.error)
```

### Validate.creator(data)

> Returns a promise that resolves to the valid object.

- **data**: `Object` *(Required)*
  a [creator](http://www.softwareishard.com/blog/har-12-spec/#creator) object

```js
validate.creator(data)
  .then(data => console.log('horray!'))
  .catch(console.error)
```

### Validate.entry(data)

> Returns a promise that resolves to the valid object.

- **data**: `Object` *(Required)*
  an [entry](http://www.softwareishard.com/blog/har-12-spec/#entries) object

```js
validate.entry(data)
  .then(data => console.log('horray!'))
  .catch(console.error)
```

### Validate.log(data)

alias of [`Validate(data)`](#validate-data-callback-)

### Validate.page(data)

> Returns a promise that resolves to the valid object.

- **data**: `Object` *(Required)*
  a [page](http://www.softwareishard.com/blog/har-12-spec/#pages) object

```js
validate.page(data)
  .then(data => console.log('horray!'))
  .catch(console.error)
```

### Validate.pageTimings(data)

> Returns a promise that resolves to the valid object.

- **data**: `Object` *(Required)*
  a [pageTimings](http://www.softwareishard.com/blog/har-12-spec/#pageTimings) object

```js
validate.pageTimings(data)
  .then(data => console.log('horray!'))
  .catch(console.error)
```

### Validate.postData(data)

> Returns a promise that resolves to the valid object.

- **data**: `Object` *(Required)*
  a [postData](http://www.softwareishard.com/blog/har-12-spec/#postData) object

```js
validate.postData(data)
  .then(data => console.log('horray!'))
  .catch(console.error)
```

### Validate.record(data)

> Returns a promise that resolves to the valid object.

- **data**: `Object` *(Required)*
  a [record](http://www.softwareishard.com/blog/har-12-spec/#headers) object

```js
validate.record(data)
  .then(data => console.log('horray!'))
  .catch(console.error)
```

### Validate.request(data)

> Returns a promise that resolves to the valid object.

- **data**: `Object` *(Required)*
  a [request](http://www.softwareishard.com/blog/har-12-spec/#request) object

```js
validate.request(data)
  .then(data => console.log('horray!'))
  .catch(console.error)
```

### Validate.response(data)

> Returns a promise that resolves to the valid object.

- **data**: `Object` *(Required)*
  a [response](http://www.softwareishard.com/blog/har-12-spec/#response) object

```js
validate.cacheEntry(data)
  .then(data => console.log('horray!'))
  .catch(console.error)
```

### Validate.timings(data)

> Returns a promise that resolves to the valid object.

- **data**: `Object` *(Required)*
  a [timings](http://www.softwareishard.com/blog/har-12-spec/#timings) object

```js
validate.timings(data)
  .then(data => console.log('horray!'))
  .catch(console.error)
```

----

## Callback API

### Validate(data [, callback])

> Returns `true` or `false`.

```js
var HAR = require('./har.json');
var validate = require('har-validator/lib/async');

validate(HAR, function (e, valid) {
  if (e) console.log(e.errors)

  if (valid) console.log('horray!');
});

```
The async API provides exactly the same methods as the [Promise API](#promise-api)

----

## Support

Donations are welcome to help support the continuous development of this project.

[![Gratipay][gratipay-image]][gratipay-url]
[![PayPal][paypal-image]][paypal-url]
[![Flattr][flattr-image]][flattr-url]
[![Bitcoin][bitcoin-image]][bitcoin-url]

## License

[ISC License](LICENSE) &copy; [Ahmad Nassri](https://www.ahmadnassri.com/)

[license-url]: https://github.com/ahmadnassri/har-validator/blob/master/LICENSE

[travis-url]: https://travis-ci.org/ahmadnassri/har-validator
[travis-image]: https://img.shields.io/travis/ahmadnassri/har-validator.svg?style=flat-square

[npm-url]: https://www.npmjs.com/package/har-validator
[npm-license]: https://img.shields.io/npm/l/har-validator.svg?style=flat-square
[npm-version]: https://img.shields.io/npm/v/har-validator.svg?style=flat-square
[npm-downloads]: https://img.shields.io/npm/dm/har-validator.svg?style=flat-square

[codeclimate-url]: https://codeclimate.com/github/ahmadnassri/har-validator
[codeclimate-quality]: https://img.shields.io/codeclimate/github/ahmadnassri/har-validator.svg?style=flat-square
[codeclimate-coverage]: https://img.shields.io/codeclimate/coverage/github/ahmadnassri/har-validator.svg?style=flat-square

[david-url]: https://david-dm.org/ahmadnassri/har-validator
[david-image]: https://img.shields.io/david/ahmadnassri/har-validator.svg?style=flat-square

[gratipay-url]: https://www.gratipay.com/ahmadnassri/
[gratipay-image]: https://img.shields.io/gratipay/ahmadnassri.svg?style=flat-square

[paypal-url]: https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=UJ2B2BTK9VLRS&on0=project&os0=har-validator
[paypal-image]: http://img.shields.io/badge/paypal-donate-green.svg?style=flat-square

[flattr-url]: https://flattr.com/submit/auto?user_id=ahmadnassri&url=https://github.com/ahmadnassri/har-validator&title=har-validator&language=&tags=github&category=software
[flattr-image]: http://img.shields.io/badge/flattr-donate-green.svg?style=flat-square

[bitcoin-image]: http://img.shields.io/badge/bitcoin-1Nb46sZRVG3or7pNaDjthcGJpWhvoPpCxy-green.svg?style=flat-square
[bitcoin-url]: https://www.coinbase.com/checkouts/ae383ae6bb931a2fa5ad11cec115191e?name=har-validator
