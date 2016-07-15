# Path

> Stability: 2 - Stable

The `path` module provides utilities for working with file and directory paths.
It can be accessed using:

```js
const path = require('path');
```

## Windows vs. POSIX

The default operation of the `path` module varies based on the operating system
on which a Node.js application is running. Specifically, when running on a
Windows operating system, the `path` module will assume that Windows-style
paths are being used.

For example, using the `path.basename()` function with the Windows file path
`C:\temp\myfile.html`, will yield different results when running on POSIX than
when run on Windows:

On POSIX:

```js
path.basename('C:\\temp\\myfile.html');
  // returns 'C:\temp\myfile.html'
```

On Windows:

```js
path.basename('C:\\temp\\myfile.html');
  // returns 'myfile.html'
```

To achieve consistent results when working with Windows file paths on any
operating system, use [`path.win32`][]:

On POSIX and Windows:

```js
path.win32.basename('C:\\temp\\myfile.html');
  // returns 'myfile.html'
```

To achieve consistent results when working with POSIX file paths on any
operating system, use [`path.posix`][]:

On POSIX and Windows:

```js
path.posix.basename('/tmp/myfile.html');
  // returns 'myfile.html'
```

## path.basename(path[, ext])
<!-- YAML
added: v0.1.25
-->

* `path` {String}
* `ext` {String} An optional file extension

The `path.basename()` methods returns the last portion of a `path`, similar to
the Unix `basename` command.

For example:

```js
path.basename('/foo/bar/baz/asdf/quux.html')
  // returns 'quux.html'

path.basename('/foo/bar/baz/asdf/quux.html', '.html')
  // returns 'quux'
```

A [`TypeError`][] is thrown if `path` is not a string or if `ext` is given
and is not a string.

## path.delimiter
<!-- YAML
added: v0.9.3
-->

Provides the platform-specific path delimiter:

* `;` for Windows
* `:` for POSIX

For example, on POSIX:

```js
console.log(process.env.PATH)
// '/usr/bin:/bin:/usr/sbin:/sbin:/usr/local/bin'

process.env.PATH.split(path.delimiter)
// returns ['/usr/bin', '/bin', '/usr/sbin', '/sbin', '/usr/local/bin']
```

On Windows:

```js
console.log(process.env.PATH)
// 'C:\Windows\system32;C:\Windows;C:\Program Files\node\'

process.env.PATH.split(path.delimiter)
// returns ['C:\\Windows\\system32', 'C:\\Windows', 'C:\\Program Files\\node\\']
```

## path.dirname(path)
<!-- YAML
added: v0.1.16
-->

* `path` {String}

The `path.dirname()` method returns the directory name of a `path`, similar to
the Unix `dirname` command.

For example:

```js
path.dirname('/foo/bar/baz/asdf/quux')
// returns '/foo/bar/baz/asdf'
```

A [`TypeError`][] is thrown if `path` is not a string.

## path.extname(path)
<!-- YAML
added: v0.1.25
-->

* `path` {String}

The `path.extname()` method returns the extension of the `path`, from the last
occurance of the `.` (period) character to end of string in the last portion of
the `path`.  If there is no `.` in the last portion of the `path`, or if the
first character of the basename of `path` (see `path.basename()`) is `.`, then
an empty string is returned.

For example:

```js
path.extname('index.html')
// returns '.html'

path.extname('index.coffee.md')
// returns '.md'

path.extname('index.')
// returns '.'

path.extname('index')
// returns ''

path.extname('.index')
// returns ''
```

A [`TypeError`][] is thrown if `path` is not a string.

## path.format(pathObject)
<!-- YAML
added: v0.11.15
-->

* `pathObject` {Object}
  * `dir` {String}
  * `root` {String} 
  * `base` {String}
  * `name` {String}
  * `ext` {String}

The `path.format()` method returns a path string from an object. This is the
opposite of [`path.parse()`][].

The following process is used when constructing the path string:

* `output` is set to an empty string.
* If `pathObject.dir` is specified, `pathObject.dir` is appended to `output`
  followed by the value of `path.sep`;
* Otherwise, if `pathObject.root` is specified, `pathObject.root` is appended
  to `output`.
* If `pathObject.base` is specified, `pathObject.base` is appended to `output`;
* Otherwise:
  * If `pathObject.name` is specified, `pathObject.name` is appended to `output`
  * If `pathObject.ext` is specified, `pathObject.ext` is appended to `output`.
* Return `output`

For example, on POSIX:

```js
// If `dir` and `base` are provided,
// `${dir}${path.sep}${base}`
// will be returned.
path.format({
  dir: '/home/user/dir',
  base: 'file.txt'
});
// returns '/home/user/dir/file.txt'

// `root` will be used if `dir` is not specified.
// If only `root` is provided or `dir` is equal to `root` then the
// platform separator will not be included.
path.format({
  root: '/',
  base: 'file.txt'
});
// returns '/file.txt'

// `name` + `ext` will be used if `base` is not specified.
path.format({
  root: '/',
  name: 'file',
  ext: '.txt'
});
// returns '/file.txt'

// `base` will be returned if `dir` or `root` are not provided.
path.format({
  base: 'file.txt'
});
// returns 'file.txt'
```

On Windows:

```js
path.format({
    root : "C:\\",
    dir : "C:\\path\\dir",
    base : "file.txt",
    ext : ".txt",
    name : "file"
});
// returns 'C:\\path\\dir\\file.txt'
```

## path.isAbsolute(path)
<!-- YAML
added: v0.11.2
-->

* `path` {String}

The `path.isAbsolute()` method determines if `path` is an absolute path.

If the given `path` is a zero-length string, `false` will be returned.

For example on POSIX:

```js
path.isAbsolute('/foo/bar') // true
path.isAbsolute('/baz/..')  // true
path.isAbsolute('qux/')     // false
path.isAbsolute('.')        // false
```

On Windows:

```js
path.isAbsolute('//server')  // true
path.isAbsolute('C:/foo/..') // true
path.isAbsolute('bar\\baz')  // false
path.isAbsolute('.')         // false
```

A [`TypeError`][] is thrown if `path` is not a string.

## path.join([path[, ...]])
<!-- YAML
added: v0.1.16
-->

* `[path[, ...]]` {String} A sequence of path segments

The `path.join()` method join all given `path` segments together using the
platform specific separator as a delimiter, then normalizes the resulting path.

Zero-length `path` segments are ignored. If the joined path string is a
zero-length string then `'.'` will be returned, representing the current
working directory.

For example:

```js
path.join('/foo', 'bar', 'baz/asdf', 'quux', '..')
// returns '/foo/bar/baz/asdf'

path.join('foo', {}, 'bar')
// throws TypeError: Arguments to path.join must be strings
```

A [`TypeError`][] is thrown if any of the path segments is not a string.

## path.normalize(path)
<!-- YAML
added: v0.1.23
-->

* `path` {String}

The `path.normalize()` method normalizes the given `path`, resolving `'..'` and
`'.'` segments.

When multiple, sequential path segment separation characters are found (e.g.
`/` on POSIX and `\` on Windows), they are replaced by a single instance of the
platform specific path segment separator. Trailing separators are preserved.

If the `path` is a zero-length string, `'.'` is returned, representing the
current working directory.

For example on POSIX:

```js
path.normalize('/foo/bar//baz/asdf/quux/..')
// returns '/foo/bar/baz/asdf'
```

On Windows:

```js
path.normalize('C:\\temp\\\\foo\\bar\\..\\');
// returns 'C:\\temp\\foo\\'
```

A [`TypeError`][] is thrown if `path` is not a string.

## path.parse(path)
<!-- YAML
added: v0.11.15
-->

* `path` {String}

The `path.parse()` method returns an object whose properties represent
significant elements of the `path`.

The returned object will have the following properties:

* `root` {String}
* `dir` {String}
* `base` {String}
* `ext` {String}
* `name` {String}

For example on POSIX:

```js
path.parse('/home/user/dir/file.txt')
// returns
// {
//    root : "/",
//    dir : "/home/user/dir",
//    base : "file.txt",
//    ext : ".txt",
//    name : "file"
// }
```

```text
┌─────────────────────┬────────────┐
│          dir        │    base    │
├──────┬              ├──────┬─────┤
│ root │              │ name │ ext │
"  /    home/user/dir / file  .txt "
└──────┴──────────────┴──────┴─────┘
(all spaces in the "" line should be ignored -- they are purely for formatting)
```

On Windows:

```js
path.parse('C:\\path\\dir\\file.txt')
// returns
// {
//    root : "C:\\",
//    dir : "C:\\path\\dir",
//    base : "file.txt",
//    ext : ".txt",
//    name : "file"
// }
```

```text
┌─────────────────────┬────────────┐
│          dir        │    base    │
├──────┬              ├──────┬─────┤
│ root │              │ name │ ext │
" C:\      path\dir   \ file  .txt "
└──────┴──────────────┴──────┴─────┘
(all spaces in the "" line should be ignored -- they are purely for formatting)
```

A [`TypeError`][] is thrown if `path` is not a string.

## path.posix
<!-- YAML
added: v0.11.15
-->

The `path.posix` property provides access to POSIX specific implementations
of the `path` methods.

## path.relative(from, to)
<!-- YAML
added: v0.5.0
-->

* `from` {String}
* `to` {String}

The `path.relative()` method returns the relative path from `from` to `to`.
If `from` and `to` each resolve to the same path (after calling `path.resolve()`
on each), a zero-length string is returned.

If a zero-length string is passed as `from` or `to`, the current working
directory will be used instead of the zero-length strings.

For example on POSIX:

```js
path.relative('/data/orandea/test/aaa', '/data/orandea/impl/bbb')
// returns '../../impl/bbb'
```

On Windows:

```js
path.relative('C:\\orandea\\test\\aaa', 'C:\\orandea\\impl\\bbb')
// returns '..\\..\\impl\\bbb'
```

A [`TypeError`][] is thrown if neither `from` nor `to` is a string.

## path.resolve([path[, ...]])
<!-- YAML
added: v0.3.4
-->

* `[path[, ...]]` {String} A sequence of paths or path segments

The `path.resolve()` method resolves a sequence of paths or path segments into
an absolute path.

The given sequence of paths is processed from right to left, with each
subsequent `path` prepended until an absolute path is constructed.
For instance, given the sequence of path segments: `/foo`, `/bar`, `baz`,
calling `path.resolve('/foo', '/bar', 'baz')` would return `/bar/baz`.

If after processing all given `path` segments an absolute path has not yet
been generated, the current working directory is used.

The resulting path is normalized and trailing slashes are removed unless the
path is resolved to the root directory.

Zero-length `path` segments are ignored.

If no `path` segments are passed, `path.resolve()` will return the absolute path
of the current working directory.

For example:

```js
path.resolve('/foo/bar', './baz')
// returns '/foo/bar/baz'

path.resolve('/foo/bar', '/tmp/file/')
// returns '/tmp/file'

path.resolve('wwwroot', 'static_files/png/', '../gif/image.gif')
// if the current working directory is /home/myself/node,
// this returns '/home/myself/node/wwwroot/static_files/gif/image.gif'
```

A [`TypeError`][] is thrown if any of the arguments is not a string.

## path.sep
<!-- YAML
added: v0.7.9
-->

Provides the platform-specific path segment separator:

* `\` on Windows
* `/` on POSIX

For example on POSIX:

```js
'foo/bar/baz'.split(path.sep)
// returns ['foo', 'bar', 'baz']
```

On Windows:

```js
'foo\\bar\\baz'.split(path.sep)
// returns ['foo', 'bar', 'baz']
```

## path.win32
<!-- YAML
added: v0.11.15
-->

The `path.win32` property provides access to Windows-specific implementations
of the `path` methods.

[`path.posix`]: #path_path_posix
[`path.win32`]: #path_path_win32
[`path.parse()`]: #path_path_parse_path
[`TypeError`]: errors.html#errors_class_typeerror
