# cross-spawn-async

[![NPM version][npm-image]][npm-url] [![Downloads][downloads-image]][npm-url] [![Build Status][travis-image]][travis-url] [![Build status][appveyor-image]][appveyor-url] [![Dependency status][david-dm-image]][david-dm-url] [![Dev Dependency status][david-dm-dev-image]][david-dm-dev-url]

[npm-url]:https://npmjs.org/package/cross-spawn-async
[downloads-image]:http://img.shields.io/npm/dm/cross-spawn-async.svg
[npm-image]:http://img.shields.io/npm/v/cross-spawn-async.svg
[travis-url]:https://travis-ci.org/IndigoUnited/node-cross-spawn-async
[travis-image]:http://img.shields.io/travis/IndigoUnited/node-cross-spawn-async/master.svg
[appveyor-url]:https://ci.appveyor.com/project/satazor/node-cross-spawn-async
[appveyor-image]:https://img.shields.io/appveyor/ci/satazor/node-cross-spawn-async/master.svg
[david-dm-url]:https://david-dm.org/IndigoUnited/node-cross-spawn-async
[david-dm-image]:https://img.shields.io/david/IndigoUnited/node-cross-spawn-async.svg
[david-dm-dev-url]:https://david-dm.org/IndigoUnited/node-cross-spawn-async#info=devDependencies
[david-dm-dev-image]:https://img.shields.io/david/dev/IndigoUnited/node-cross-spawn-async.svg

A cross platform solution to node's spawn.

**This module is deprecated, use [cross-spawn](https://github.com/IndigoUnited/node-cross-spawn) instead which no longer requires a build toolchain.**


## Installation

`$ npm install cross-spawn-async`


## Why

Node has issues when using spawn on Windows:

- It ignores [PATHEXT](https://github.com/joyent/node/issues/2318)
- It does not support [shebangs](http://pt.wikipedia.org/wiki/Shebang)
- It does not allow you to run `del` or `dir`
- It does not properly escape arguments with spaces or special characters

All these issues are handled correctly by `cross-spawn-async`.
There are some known modules, such as [win-spawn](https://github.com/ForbesLindesay/win-spawn), that try to solve this but they are either broken or provide faulty escaping of shell arguments.


## Usage

Exactly the same way as node's [`spawn`](https://nodejs.org/api/child_process.html#child_process_child_process_spawn_command_args_options), so it's a drop in replacement.

```javascript
var spawn = require('cross-spawn-async');

var child = spawn('npm', ['list', '-g', '-depth', '0'], { stdio: 'inherit' });
```


## Tests

`$ npm test`


## License

Released under the [MIT License](http://www.opensource.org/licenses/mit-license.php).
