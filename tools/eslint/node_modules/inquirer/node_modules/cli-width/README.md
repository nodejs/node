cli-width
=========

Get stdout window width, with two fallbacks, `tty` and then a default.

## Usage

```
npm install --save cli-width
```

```js
'use stict';

var cliWidth = require('cli-width');

cliWidth(); // maybe 204 :)
```

If none of the methods are supported, the default is `0` and
can be changed via `cliWidth.defaultWidth = 200;`.

## Tests

```bash
npm install
npm test
```
