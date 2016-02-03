# log-update [![Build Status](https://travis-ci.org/sindresorhus/log-update.svg?branch=master)](https://travis-ci.org/sindresorhus/log-update)

> Log by overwriting the previous output in the terminal.  
> Useful for rendering progress bars, animations, etc.

![](screenshot.gif)


## Install

```
$ npm install --save log-update
```


## Usage

```js
var logUpdate = require('log-update');

var i = 0;
var frames = ['-', '\\', '|', '/'];

setInterval(function () {
	var frame = frames[i++ % frames.length];
	logUpdate('\n' + '        ♥♥\n   ' + frame + ' unicorns ' + frame + '\n        ♥♥');
}, 100);
```


## API

### logUpdate(text, ...)

Log to stdout.

### logUpdate.clear()

Clear the logged output.

### logUpdate.done()

Persist the logged output.  
Useful if you want to start a new log session below the current one.

### logUpdate.stderr(text, ...)

Log to stderr.

### logUpdate.stderr.clear()
### logUpdate.stderr.done()

### logUpdate.create(stream)

Get a `logUpdate` method that logs to the specified stream.


## Examples

- [speed-test](https://github.com/sindresorhus/speed-test) - Uses this module to render a spinner


## License

MIT © [Sindre Sorhus](http://sindresorhus.com)
