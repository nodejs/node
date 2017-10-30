# widest-line [![Build Status](https://travis-ci.org/sindresorhus/widest-line.svg?branch=master)](https://travis-ci.org/sindresorhus/widest-line)

> Get the visual width of the widest line in a string - the number of columns required to display it

Some Unicode characters are [fullwidth](https://en.wikipedia.org/wiki/Halfwidth_and_fullwidth_forms) and use double the normal width. [ANSI escape codes](http://en.wikipedia.org/wiki/ANSI_escape_code) are stripped and doesn't affect the width.

Useful to be able to know the maximum width a string will take up in the terminal.


## Install

```
$ npm install --save widest-line
```


## Usage

```js
const widestLine = require('widest-line');

widestLine('古\n\u001b[1m@\u001b[22m');
//=> 2
```


## Related

- [string-width](https://github.com/sindresorhus/string-width) - Get the visual width of a string


## License

MIT © [Sindre Sorhus](http://sindresorhus.com)
