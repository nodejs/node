# execa [![Build Status: Linux](https://travis-ci.org/sindresorhus/execa.svg?branch=master)](https://travis-ci.org/sindresorhus/execa) [![Build status: Windows](https://ci.appveyor.com/api/projects/status/x5ajamxtjtt93cqv/branch/master?svg=true)](https://ci.appveyor.com/project/sindresorhus/execa/branch/master) [![Coverage Status](https://coveralls.io/repos/github/sindresorhus/execa/badge.svg?branch=master)](https://coveralls.io/github/sindresorhus/execa?branch=master)

> A better [`child_process`](https://nodejs.org/api/child_process.html)


## Why

- Promise interface.
- [Strips EOF](https://github.com/sindresorhus/strip-eof) from the output so you don't have to `stdout.trim()`.
- Supports [shebang](https://en.wikipedia.org/wiki/Shebang_(Unix)) binaries cross-platform.
- [Improved Windows support.](https://github.com/IndigoUnited/node-cross-spawn-async#why)
- Higher max buffer. 10 MB instead of 200 KB.
- [Executes locally installed binaries by name.](#preferlocal)


## Install

```
$ npm install --save execa
```


## Usage

```js
const execa = require('execa');

execa('echo', ['unicorns']).then(result => {
	console.log(result.stdout);
	//=> 'unicorns'
});

execa.shell('echo unicorns').then(result => {
	console.log(result.stdout);
	//=> 'unicorns'
});

// example of catching an error
execa.shell('exit 3').catch(error => {
	console.log(error);
	/*
	{
		message: 'Command failed: /bin/sh -c exit 3'
		killed: false,
		code: 3,
		signal: null,
		cmd: '/bin/sh -c exit 3',
		stdout: '',
		stderr: ''
	}
	*/
});
```


## API

### execa(file, [arguments], [options])

Execute a file.

Same options as [`child_process.execFile`](https://nodejs.org/api/child_process.html#child_process_child_process_execfile_file_args_options_callback).

Returns a [`child_process` instance](https://nodejs.org/api/child_process.html#child_process_class_childprocess).

The `child_process` instance is enhanced to also be promise for a result object with `stdout` and `stderr` properties.

### execa.stdout(file, [arguments], [options])

Same as `execa()`, but returns only `stdout`.

### execa.stderr(file, [arguments], [options])

Same as `execa()`, but returns only `stderr`.

### execa.shell(command, [options])

Execute a command through the system shell. Prefer `execa()` whenever possible, as it's both faster and safer.

Same options as [`child_process.exec`](https://nodejs.org/api/child_process.html#child_process_child_process_exec_command_options_callback).

Returns a [`child_process` instance](https://nodejs.org/api/child_process.html#child_process_class_childprocess).

The `child_process` instance is enhanced to also be promise for a result object with `stdout` and `stderr` properties.

### execa.spawn(file, [arguments], [options])

Spawn a file.

Same API as [`child_process.spawn`](https://nodejs.org/api/child_process.html#child_process_child_process_spawn_command_args_options).

### execa.sync(file, [arguments], [options])

Execute a file synchronously.

Same options as [`child_process.execFileSync`](https://nodejs.org/api/child_process.html#child_process_child_process_execfilesync_file_args_options), except the default encoding is `utf8` instead of `buffer`.

Returns the same result object as [`child_process.spawnSync`](https://nodejs.org/api/child_process.html#child_process_child_process_spawnsync_command_args_options).

### execa.shellSync(file, [options])

Execute a command synchronously through the system shell.

Same options as [`child_process.execSync`](https://nodejs.org/api/child_process.html#child_process_child_process_execsync_command_options), except the default encoding is `utf8` instead of `buffer`.

Returns the same result object as [`child_process.spawnSync`](https://nodejs.org/api/child_process.html#child_process_child_process_spawnsync_command_args_options).

### options

Additional options:

#### stripEof

Type: `boolean`<br>
Default: `true`

[Strip EOF](https://github.com/sindresorhus/strip-eof) (last newline) from the output.

#### preferLocal

Type: `boolean`<br>
Default: `true`

Prefer locally installed binaries when looking for a binary to execute.<br>
If you `$ npm install foo`, you can then `execa('foo')`.

#### input

Type: `string` `Buffer` `ReadableStream`

Write some input to the `stdin` of your binary.<br>
Streams are not allowed when using the synchronous methods.


## License

MIT © [Sindre Sorhus](https://sindresorhus.com)
