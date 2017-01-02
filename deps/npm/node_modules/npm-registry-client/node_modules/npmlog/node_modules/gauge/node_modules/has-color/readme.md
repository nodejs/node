# has-color [![Build Status](https://travis-ci.org/sindresorhus/has-color.svg?branch=master)](https://travis-ci.org/sindresorhus/has-color)

> Detect whether a terminal supports color.

Used in the terminal color module [chalk](https://github.com/sindresorhus/chalk).


## Install

```bash
$ npm install --save has-color
```


## Usage

```js
var hasColor = require('has-color');

if (hasColor) {
	console.log('Terminal supports color.');
}
```

It obeys the `--color` and `--no-color` CLI flags.


## License

[MIT](http://opensource.org/licenses/MIT) © [Sindre Sorhus](http://sindresorhus.com)
