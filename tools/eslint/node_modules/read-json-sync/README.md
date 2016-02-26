# read-json-sync

[![NPM version](https://img.shields.io/npm/v/read-json-sync.svg)](https://www.npmjs.com/package/read-json-sync)
[![Build Status](https://travis-ci.org/shinnn/read-json-sync.svg?branch=master)](https://travis-ci.org/shinnn/read-json-sync)
[![Build status](https://ci.appveyor.com/api/projects/status/t7sjgpku9on12d32?svg=true)](https://ci.appveyor.com/project/ShinnosukeWatanabe/read-json-sync)
[![Coverage Status](https://img.shields.io/coveralls/shinnn/read-json-sync.svg)](https://david-dm.org/shinnn/read-json-sync)
[![Dependency Status](https://david-dm.org/shinnn/read-json-sync.svg)](https://david-dm.org/shinnn/read-json-sync)
[![devDependency Status](https://david-dm.org/shinnn/read-json-sync/dev-status.svg)](https://david-dm.org/shinnn/read-json-sync#info=devDependencies)

A [Node](https://nodejs.org/) module to read and parse a [JSON](http://www.json.org/) file synchronously

```javascript
const readJsonSync = require('read-json-sync');

readJsonSync('package.json'); //=> {name: 'read-json-sync', version: '1.0.0', ...}
```

Node's built-in [`require`](https://nodejs.org/api/globals.html#globals_require) can do almost the same thing, but this module doesn't [cache](https://nodejs.org/api/modules.html#modules_caching) results.

## Installation

[Use npm](https://docs.npmjs.com/cli/install).

```
npm install read-json-sync
```

## API

```javascript
const readJsonSync = require('read-json-sync');
```

### readJsonSync(*filePath* [, *options*])

*filePath*: `String` (path to a JSON file)  
*options*: `Object` ([`fs.readFile`](https://nodejs.org/api/fs.html#fs_fs_readfile_filename_options_callback) options)  
Return: `Object` (parsed [JSON](https://tools.ietf.org/html/rfc7159) data)

## License

Copyright (c) 2014 - 2015 [Shinnosuke Watanabe](https://github.com/shinnn)

Licensed under [the MIT License](./LICENSE).
