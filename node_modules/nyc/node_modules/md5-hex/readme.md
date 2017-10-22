# md5-hex [![Build Status](https://travis-ci.org/sindresorhus/md5-hex.svg?branch=master)](https://travis-ci.org/sindresorhus/md5-hex)

> Create a MD5 hash with hex encoding

*Please don't use MD5 hashes for anything sensitive!*

Checkout [`hasha`](https://github.com/sindresorhus/hasha) if you need something more flexible.


## Install

```
$ npm install --save md5-hex
```


## Usage

```js
const fs = require('fs');
const md5Hex = require('md5-hex');
const buffer = fs.readFileSync('unicorn.png');

md5Hex(buffer);
//=> '1abcb33beeb811dca15f0ac3e47b88d9'
```


## API

### md5Hex(input)

#### input

Type: `buffer` `string` `array[string|buffer]`

Prefer buffers as they're faster to hash, but strings can be useful for small things.

Pass an array instead of concatenating strings and/or buffers. The output is the same, but arrays do not incur the overhead of concatenation.


## License

MIT © [Sindre Sorhus](https://sindresorhus.com)
