# sha

Check and get file hashes (using any algorithm)

[![Build Status](https://travis-ci.org/ForbesLindesay/sha.png?branch=master)](https://travis-ci.org/ForbesLindesay/sha)
[![Dependency Status](https://gemnasium.com/ForbesLindesay/sha.png)](https://gemnasium.com/ForbesLindesay/sha)
[![NPM version](https://badge.fury.io/js/sha.png)](http://badge.fury.io/js/sha)

## Installation

    $ npm install sha

## API

### check(fileName, expected, [options,] cb) / checkSync(filename, expected, [options])

Asynchronously check that `fileName` has a "hash" of `expected`.  The callback will be called with either `null` or an error (indicating that they did not match).

Options:

- algorithm: defaults to `sha1` and can be any of the algorithms supported by `crypto.createHash`

### get(fileName, [options,] cb) / getSync(filename, [options])

Asynchronously get the "hash" of `fileName`.  The callback will be called with an optional `error` object and the (lower cased) hex digest of the hash.

Options:

- algorithm: defaults to `sha1` and can be any of the algorithms supported by `crypto.createHash`

### stream(expected, [options])

Check the hash of a stream without ever buffering it.  This is a pass through stream so you can do things like:

```js
fs.createReadStream('src')
  .pipe(sha.stream('expected'))
  .pipe(fs.createWriteStream('dest'))
```

`dest` will be a complete copy of `src` and an error will be emitted if the hash did not match `'expected'`.

Options:

- algorithm: defaults to `sha1` and can be any of the algorithms supported by `crypto.createHash`

## License

You may use this software under the BSD or MIT.  Take your pick.  If you want me to release it under another license, open a pull request.