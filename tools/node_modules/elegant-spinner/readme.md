# elegant-spinner [![Build Status](https://travis-ci.org/sindresorhus/elegant-spinner.svg?branch=master)](https://travis-ci.org/sindresorhus/elegant-spinner)

> Elegant spinner for interactive CLI apps

<img width="173" src="screenshot.gif">


## Install

```
$ npm install --save elegant-spinner
```


## Usage

```js
var elegantSpinner = require('elegant-spinner');
var logUpdate = require('log-update');
var frame = elegantSpinner();

setInterval(function () {
	logUpdate(frame());
}, 50);
```


## Relevant

- [log-update](https://github.com/sindresorhus/log-update) - Log by overwriting the previous output in the terminal. Useful for rendering progress bars, animations, etc.


## License

MIT © [Sindre Sorhus](http://sindresorhus.com)
