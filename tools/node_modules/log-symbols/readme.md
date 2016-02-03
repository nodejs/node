# log-symbols [![Build Status](https://travis-ci.org/sindresorhus/log-symbols.svg?branch=master)](https://travis-ci.org/sindresorhus/log-symbols)

> Colored symbols for various log levels

Includes fallbacks for Windows CMD which only supports a [limited character set](http://en.wikipedia.org/wiki/Code_page_437).

![](screenshot.png)


## Install

```sh
$ npm install --save log-symbols
```


## Usage

```js
var logSymbols = require('log-symbols');

console.log(logSymbols.success, 'finished successfully!');
// On real OSes:  ✔ finished successfully!
// On Windows:    √ finished successfully!
```

## API

### logSymbols

#### info
#### success
#### warning
#### error


## License

MIT © [Sindre Sorhus](http://sindresorhus.com)
