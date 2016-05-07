# is-resolvable

[![NPM version](https://img.shields.io/npm/v/is-resolvable.svg)](https://www.npmjs.com/package/is-resolvable)
[![Build Status](https://travis-ci.org/shinnn/is-resolvable.svg?branch=master)](https://travis-ci.org/shinnn/is-resolvable)
[![Build status](https://ci.appveyor.com/api/projects/status/ww1cdpignehlasbs?svg=true)](https://ci.appveyor.com/project/ShinnosukeWatanabe/is-resolvable)
[![Coverage Status](https://img.shields.io/coveralls/shinnn/is-resolvable.svg)](https://coveralls.io/r/shinnn/is-resolvable)
[![Dependency Status](https://img.shields.io/david/shinnn/is-resolvable.svg?label=deps)](https://david-dm.org/shinnn/is-resolvable)
[![devDependency Status](https://img.shields.io/david/dev/shinnn/is-resolvable.svg?label=devDeps)](https://david-dm.org/shinnn/is-resolvable#info=devDependencies)

A [Node](https://nodejs.org/) module to check if a module ID is resolvable with [`require()`](https://nodejs.org/api/globals.html#globals_require)

```javascript
const isResolvable = require('is-resolvable');

isResolvable('fs'); //=> true
isResolvable('path'); //=> true

// When `./index.js` exists
isResolvable('./index.js') //=> true
isResolvable('./index') //=> true
isResolvable('.') //=> true
```

## Installation

[Use npm.](https://docs.npmjs.com/cli/install)

```
npm install is-resolvable
```

## API

```javascript
const isResolvable = require('is-resolvable');
```

### isResolvable(*moduleId*)

*moduleId*: `String` (module ID)  
Return: `Boolean`

It returns `true` if `require()` can load a file form a given module ID, otherwise `false`.

```javascript
const isResolvable = require('is-resolvable');

// When `./foo.json` exists
isResolvable('./foo.json'); //=> true
isResolvable('./foo'); //=> true

isResolvable('./foo.js'); //=> false


// When `lodash` module is installed but `underscore` isn't
isResolvable('lodash'); //=> true
isResolvable('underscore'); //=> false

// When `readable-stream` module is installed
isResolvable('readable-stream/readable'); //=> true
isResolvable('readable-stream/writable'); //=> true
```

## License

Copyright (c) 2014 - 2015 [Shinnosuke Watanabe](https://github.com/shinnn)

Licensed under [the MIT License](./LICENSE).
