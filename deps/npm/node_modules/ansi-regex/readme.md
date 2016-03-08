# ansi-regex [![Build Status](https://travis-ci.org/sindresorhus/ansi-regex.svg?branch=master)](https://travis-ci.org/sindresorhus/ansi-regex)

> Regular expression for matching [ANSI escape codes](http://en.wikipedia.org/wiki/ANSI_escape_code)


## Install

```
$ npm install --save ansi-regex
```


## Usage

```js
var ansiRegex = require('ansi-regex');

ansiRegex().test('\u001b[4mcake\u001b[0m');
//=> true

ansiRegex().test('cake');
//=> false

'\u001b[4mcake\u001b[0m'.match(ansiRegex());
//=> ['\u001b[4m', '\u001b[0m']
```


## License

MIT © [Sindre Sorhus](http://sindresorhus.com)
