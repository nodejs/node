cli-width
=========

Get stdout window width, with three fallbacks, `tty`, a custom environment variable and then a default.

## Usage

```
npm install --save cli-width
```

```js
'use stict';

var cliWidth = require('cli-width');

cliWidth(); // maybe 204 :)
```

You can also set the `CLI_WIDTH` environment variable.

If none of the methods are supported, the default is `0` and
can be changed via `cliWidth.defaultWidth = 200;`.

## Tests

```bash
npm install
npm test
```
