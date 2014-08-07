# ansi-regex [![Build Status](https://travis-ci.org/sindresorhus/ansi-regex.svg?branch=master)](https://travis-ci.org/sindresorhus/ansi-regex)

> Regular expression for matching [ANSI escape codes](http://en.wikipedia.org/wiki/ANSI_escape_code)


## Install

```sh
$ npm install --save ansi-regex
```


## Usage

```js
var ansiRegex = require('ansi-regex');

ansiRegex.test('\x1b[4mcake\x1b[0m');
//=> true

ansiRegex.test('cake');
//=> false
```


## License

MIT © [Sindre Sorhus](http://sindresorhus.com)
