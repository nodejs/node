# sha

Check and get file hashes (using any algorithm)

[![Build Status](https://travis-ci.org/ForbesLindesay/sha.png?branch=master)](https://travis-ci.org/ForbesLindesay/sha)
[![Dependency Status](https://gemnasium.com/ForbesLindesay/sha.png)](https://gemnasium.com/ForbesLindesay/sha)

## Installation

    $ npm install sha

## API

### check(fileName, expected, [options,] cb)

Asynchronously check that `fileName` has a "hash" of `expected`.  The callback will be called with either `null` or an error (indicating that they did not match).

Options:

- algorithm: defaults to `sha1` and can be any of the algorithms supported by `crypto.createHash`

### get(fileName, [options,] cb)

Asynchronously get the "hash" of `fileName`.  The callback will be called with an optional `error` object and the (lower cased) hex digest of the hash.

Options:

- algorithm: defaults to `sha1` and can be any of the algorithms supported by `crypto.createHash`

## License

BSD
