# Path

    Stability: 2 - Stable

This module contains utilities for handling and transforming file
paths.  Almost all these methods perform only string transformations.
The file system is not consulted to check whether paths are valid.

Use `require('path')` to use this module.  The following methods are provided:

## path.basename(p[, ext])
<!-- YAML
added: v0.1.25
-->

Return the last portion of a path.  Similar to the Unix `basename` command.

Example:

```js
path.basename('/foo/bar/baz/asdf/quux.html')
// returns 'quux.html'

path.basename('/foo/bar/baz/asdf/quux.html', '.html')
// returns 'quux'
```

## path.delimiter
<!-- YAML
added: v0.9.3
-->

The platform-specific path delimiter, `;` or `':'`.

An example on \*nix:

```js
console.log(process.env.PATH)
// '/usr/bin:/bin:/usr/sbin:/sbin:/usr/local/bin'

process.env.PATH.split(path.delimiter)
// returns ['/usr/bin', '/bin', '/usr/sbin', '/sbin', '/usr/local/bin']
```

An example on Windows:

```js
console.log(process.env.PATH)
// 'C:\Windows\system32;C:\Windows;C:\Program Files\node\'

process.env.PATH.split(path.delimiter)
// returns ['C:\\Windows\\system32', 'C:\\Windows', 'C:\\Program Files\\node\\']
```

## path.dirname(p)
<!-- YAML
added: v0.1.16
-->

Return the directory name of a path.  Similar to the Unix `dirname` command.

Example:

```js
path.dirname('/foo/bar/baz/asdf/quux')
// returns '/foo/bar/baz/asdf'
```

## path.extname(p)
<!-- YAML
added: v0.1.25
-->

Return the extension of the path, from the last '.' to end of string
in the last portion of the path.  If there is no '.' in the last portion
of the path or the first character of it is '.', then it returns
an empty string.  Examples:

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

## path.format(pathObject)
<!-- YAML
added: v0.11.15
-->

Returns a path string from an object, the opposite of [`path.parse`][].

Examples:

Some Posix system examples:

```js
// If `dir` and `base` are provided, `dir` + platform separator + `base`
// will be returned.
path.format({
    dir: '/home/user/dir',
    base: 'file.txt'
});
// returns '/home/user/dir/file.txt'

// `base` will be returned if `dir` or `root` are not provided.
path.format({
    base: 'file.txt'
});
// returns 'file.txt'
```

## path.isAbsolute(path)
<!-- YAML
added: v0.11.2
-->

Determines whether `path` is an absolute path. An absolute path will always
resolve to the same location, regardless of the working directory.

Posix examples:

```js
path.isAbsolute('/foo/bar') // true
path.isAbsolute('/baz/..')  // true
path.isAbsolute('qux/')     // false
path.isAbsolute('.')        // false
```

Windows examples:

```js
path.isAbsolute('//server')  // true
path.isAbsolute('C:/foo/..') // true
path.isAbsolute('bar\\baz')  // false
path.isAbsolute('.')         // false
```

*Note:* If the path string passed as parameter is a zero-length string, unlike
        other path module functions, it will be used as-is and `false` will be
        returned.

## path.join([path1][, path2][, ...])
<!-- YAML
added: v0.1.16
-->

Join all arguments together and normalize the resulting path.

Arguments must be strings.  In v0.8, non-string arguments were
silently ignored.  In v0.10 and up, an exception is thrown.

Example:

```js
path.join('/foo', 'bar', 'baz/asdf', 'quux', '..')
// returns '/foo/bar/baz/asdf'

path.join('foo', {}, 'bar')
// throws exception
TypeError: Arguments to path.join must be strings
```

*Note:* If the arguments to `join` have zero-length strings, unlike other path
        module functions, they will be ignored. If the joined path string is a
        zero-length string then `'.'` will be returned, which represents the
        current working directory.

## path.normalize(p)
<!-- YAML
added: v0.1.23
-->

Normalize a string path, taking care of `'..'` and `'.'` parts.

When multiple slashes are found, they're replaced by a single one;
when the path contains a trailing slash, it is preserved.
On Windows backslashes are used.

Example:

```js
path.normalize('/foo/bar//baz/asdf/quux/..')
// returns '/foo/bar/baz/asdf'
```

*Note:* If the path string passed as argument is a zero-length string then `'.'`
        will be returned, which represents the current working directory.

## path.parse(pathString)
<!-- YAML
added: v0.11.15
-->

Returns an object from a path string.

An example on \*nix:

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

An example on Windows:

```js
path.parse('C:\\path\\dir\\index.html')
// returns
// {
//    root : "C:\\",
//    dir : "C:\\path\\dir",
//    base : "index.html",
//    ext : ".html",
//    name : "index"
// }
```

## path.posix
<!-- YAML
added: v0.11.15
-->

Provide access to aforementioned `path` methods but always interact in a posix
compatible way.

## path.relative(from, to)
<!-- YAML
added: v0.5.0
-->

Solve the relative path from `from` to `to`.

At times we have two absolute paths, and we need to derive the relative
path from one to the other.  This is actually the reverse transform of
`path.resolve`, which means we see that:

```js
path.resolve(from, path.relative(from, to)) == path.resolve(to)
```

Examples:

```js
path.relative('C:\\orandea\\test\\aaa', 'C:\\orandea\\impl\\bbb')
// returns '..\\..\\impl\\bbb'

path.relative('/data/orandea/test/aaa', '/data/orandea/impl/bbb')
// returns '../../impl/bbb'
```

*Note:* If the arguments to `relative` have zero-length strings then the current
        working directory will be used instead of the zero-length strings. If
        both the paths are the same then a zero-length string will be returned.

## path.resolve([from ...], to)
<!-- YAML
added: v0.3.4
-->

Resolves `to` to an absolute path.

If `to` isn't already absolute `from` arguments are prepended in right to left
order, until an absolute path is found. If after using all `from` paths still
no absolute path is found, the current working directory is used as well. The
resulting path is normalized, and trailing slashes are removed unless the path
gets resolved to the root directory. Non-string `from` arguments are ignored.

Another way to think of it is as a sequence of `cd` commands in a shell.

```js
path.resolve('foo/bar', '/tmp/file/', '..', 'a/../subfile')
```

Is similar to:

```
cd foo/bar
cd /tmp/file/
cd ..
cd a/../subfile
pwd
```

The difference is that the different paths don't need to exist and may also be
files.

Examples:

```js
path.resolve('/foo/bar', './baz')
// returns '/foo/bar/baz'

path.resolve('/foo/bar', '/tmp/file/')
// returns '/tmp/file'

path.resolve('wwwroot', 'static_files/png/', '../gif/image.gif')
// if currently in /home/myself/node, it returns
// '/home/myself/node/wwwroot/static_files/gif/image.gif'
```

*Note:* If the arguments to `resolve` have zero-length strings then the current
        working directory will be used instead of them.

## path.sep
<!-- YAML
added: v0.7.9
-->

The platform-specific file separator. `'\\'` or `'/'`.

An example on \*nix:

```js
'foo/bar/baz'.split(path.sep)
// returns ['foo', 'bar', 'baz']
```

An example on Windows:

```js
'foo\\bar\\baz'.split(path.sep)
// returns ['foo', 'bar', 'baz']
```

## path.win32
<!-- YAML
added: v0.11.15
-->

Provide access to aforementioned `path` methods but always interact in a win32
compatible way.

[`path.parse`]: #path_path_parse_pathstring
