# env-paths [![Build Status](https://travis-ci.org/sindresorhus/env-paths.svg?branch=master)](https://travis-ci.org/sindresorhus/env-paths)

> Get paths for storing things like data, config, cache, etc

Uses the correct OS-specific paths. Most developers get this wrong.


## Install

```
$ npm install env-paths
```


## Usage

```js
const envPaths = require('env-paths');

const paths = envPaths('MyApp');

paths.data;
//=> '/home/sindresorhus/.local/share/MyApp-nodejs'

paths.config
//=> '/home/sindresorhus/.config/MyApp-nodejs'
```


## API

### paths = envPaths(name, [options])

#### name

Type: `string`

Name of your project. Used to generate the paths.

#### options

Type: `Object`

##### suffix

Type: `string`<br>
Default: `'nodejs'`

**Don't use this option unless you really have to!**<br>
Suffix appended to the project name to avoid name conflicts with native
apps. Pass an empty string to disable it.

### paths.data

Directory for data files.

### paths.config

Directory for config files.

### paths.cache

Directory for non-essential data files.

### paths.log

Directory for log files.

### paths.temp

Directory for temporary files.


## License

MIT © [Sindre Sorhus](https://sindresorhus.com)
