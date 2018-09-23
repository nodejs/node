# strip-ansi [![Build Status](https://travis-ci.org/sindresorhus/strip-ansi.svg?branch=master)](https://travis-ci.org/sindresorhus/strip-ansi)

> Strip [ANSI escape codes](http://en.wikipedia.org/wiki/ANSI_escape_code)


## Install

```
$ npm install --save strip-ansi
```


## Usage

```js
var stripAnsi = require('strip-ansi');

stripAnsi('\u001b[4mcake\u001b[0m');
//=> 'cake'
```


## Related

- [strip-ansi-cli](https://github.com/sindresorhus/strip-ansi-cli) - CLI for this module
- [has-ansi](https://github.com/sindresorhus/has-ansi) - Check if a string has ANSI escape codes
- [ansi-regex](https://github.com/sindresorhus/ansi-regex) - Regular expression for matching ANSI escape codes
- [chalk](https://github.com/sindresorhus/chalk) - Terminal string styling done right


## License

MIT © [Sindre Sorhus](http://sindresorhus.com)
