# wrap-ansi [![Build Status](https://travis-ci.org/chalk/wrap-ansi.svg?branch=master)](https://travis-ci.org/chalk/wrap-ansi) [![Coverage Status](https://coveralls.io/repos/chalk/wrap-ansi/badge.svg?branch=master&service=github)](https://coveralls.io/github/chalk/wrap-ansi?branch=master)

> Wordwrap a string with [ANSI escape codes](http://en.wikipedia.org/wiki/ANSI_escape_code#Colors_and_Styles)


## Install

```
$ npm install --save wrap-ansi
```


## Usage

```js
const chalk = require('chalk');
const wrapAnsi = require('wrap-ansi');

const input = 'The quick brown ' + chalk.red('fox jumped over ') +
	'the lazy ' + chalk.green('dog and then ran away with the unicorn.');

console.log(wrapAnsi(input, 20));
```

<img width="331" src="screenshot.png">


## API

### wrapAnsi(input, columns, [options])

Wrap words to the specified column width.

#### input

Type: `string`

String with ANSI escape codes. Like one styled by [`chalk`](https://github.com/chalk/chalk).

#### columns

Type: `number`

Number of columns to wrap the text to.

#### options.hard

Type: `boolean`  
Default: `false`

By default the wrap is soft, meaning long words may extend past the column width. Setting this to `true` will make it hard wrap at the column width.


## Related

- [slice-ansi](https://github.com/chalk/slice-ansi) - Slice a string with ANSI escape codes
- [chalk](https://github.com/chalk/chalk) - Terminal string styling done right
- [jsesc](https://github.com/mathiasbynens/jsesc) - Generate ASCII-only output from Unicode strings. Useful for creating test fixtures.


## License

MIT © [Sindre Sorhus](http://sindresorhus.com)
