# to-vfile [![Build Status](https://img.shields.io/travis/wooorm/to-vfile.svg)](https://travis-ci.org/wooorm/to-vfile) [![Coverage Status](https://img.shields.io/codecov/c/github/wooorm/to-vfile.svg)](https://codecov.io/github/wooorm/to-vfile)

Create a [vfile](https://github.com/wooorm/vfile) from a file-path.

## Installation

[npm](https://docs.npmjs.com/cli/install):

```bash
npm install to-vfile
```

**to-vfile** is also available for [bower](http://bower.io/#install-packages),
[component](https://github.com/componentjs/component), and [duo](http://duojs.org/#getting-started),
and as an AMD, CommonJS, and globals module, [uncompressed](to-vfile.js) and [compressed](to-vfile.min.js).

> **Note:** browser-builds do not include `read` and `readSync`.

## Usage

```js
var toVFile = require('to-vfile');

var file = toVFile('./readme.md');
/*
 * { contents: '',
 *   filename: 'readme',
 *   directory: '.',
 *   extension: 'md',
 *   messages: [],
 *   __proto__: [VFile] }
 */

toVFile.read('.git/HEAD', function (err, file) {
    if (err) throw err;

    console.log(file);
    /*
     * { contents: 'ref: refs/heads/master\n',
     *   filename: 'HEAD',
     *   directory: '.git',
     *   extension: '',
     *   messages: [],
     *   __proto__: [VFile] }
     */
});

var file = toVFile.readSync('.gitignore')
/*
 * { contents: '.DS_Store\n*.log\nbower_components/\nbuild/\ncomponents/\nnode_modules/\ncoverage/\nbuild.js\n',
 *   filename: '.gitignore',
 *   directory: '.',
 *   extension: '',
 *   messages: [],
 *   __proto__: [VFile] }
 */
```

## API

### toVFile(filePath)

Create a virtual file from `filePath`.

**Signatures**

*   `file = toVFile(filePath)`.

**Parameters**

*   `filePath` (`string`) — Path to a (possibly non-existent) file;

**Returns**

[`vfile`](https://github.com/wooorm/vfile) — Instance.

### toVFile.read(filePath, done)

Create a virtual file from `filePath` and fill it with the actual contents
at `filePath`.

**Signatures**

*   `toVFile.read(filePath, callback)`.

**Parameters**

*   `filePath` (`string`) — Path to a (possibly non-existent) file;
*   `callback` — See [`function done(err, vfile)`](#function-doneerr-vfile).

**Returns**

[`vfile`](https://github.com/wooorm/vfile) — Instance.

#### function done(err, vfile)

Callback.

**Signatures**

*   `function done(Error, null)`;
*   `function done(null, VFile)`.

**Parameters**

*   `err` (`Error`) — Error from reading `filePath`;
*   `vfile` (`VFile`) — Virtual file.

### toVFile.readSync(filePath)

Create a virtual file from `filePath` and fill it with the actual contents at
`filePath` (synchroneously).

**Signatures**

*   `toVFile.read(filePath, callback)`.

**Parameters**

*   `filePath` (`string`) — Path to a (possibly non-existent) file;

**Returns**

[`vfile`](https://github.com/wooorm/vfile) — Instance.

**Throw**

`Error` — When reading `filePath` fails.

## License

[MIT](LICENSE) © [Titus Wormer](http://wooorm.com)
