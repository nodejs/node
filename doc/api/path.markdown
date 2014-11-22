# Path

    Stability: 3 - Stable

This module contains utilities for handling and transforming file
paths.  Almost all these methods perform only string transformations.
The file system is not consulted to check whether paths are valid.

Use `require('path')` to use this module.  The following methods are provided:

## path.normalize(p)

Normalize a string path, taking care of `'..'` and `'.'` parts.

When multiple slashes are found, they're replaced by a single one;
when the path contains a trailing slash, it is preserved.
On Windows backslashes are used.

Example:

    path.normalize('/foo/bar//baz/asdf/quux/..')
    // returns
    '/foo/bar/baz/asdf'

## path.join([path1][, path2][, ...])

Join all arguments together and normalize the resulting path.

Arguments must be strings.  In v0.8, non-string arguments were
silently ignored.  In v0.10 and up, an exception is thrown.

Example:

    path.join('/foo', 'bar', 'baz/asdf', 'quux', '..')
    // returns
    '/foo/bar/baz/asdf'

    path.join('foo', {}, 'bar')
    // throws exception
    TypeError: Arguments to path.join must be strings

## path.resolve([from ...], to)

Resolves `to` to an absolute path.

If `to` isn't already absolute `from` arguments are prepended in right to left
order, until an absolute path is found. If after using all `from` paths still
no absolute path is found, the current working directory is used as well. The
resulting path is normalized, and trailing slashes are removed unless the path
gets resolved to the root directory. Non-string `from` arguments are ignored.

Another way to think of it is as a sequence of `cd` commands in a shell.

    path.resolve('foo/bar', '/tmp/file/', '..', 'a/../subfile')

Is similar to:

    cd foo/bar
    cd /tmp/file/
    cd ..
    cd a/../subfile
    pwd

The difference is that the different paths don't need to exist and may also be
files.

Examples:

    path.resolve('/foo/bar', './baz')
    // returns
    '/foo/bar/baz'

    path.resolve('/foo/bar', '/tmp/file/')
    // returns
    '/tmp/file'

    path.resolve('wwwroot', 'static_files/png/', '../gif/image.gif')
    // if currently in /home/myself/node, it returns
    '/home/myself/node/wwwroot/static_files/gif/image.gif'

## path.isAbsolute(path)

Determines whether `path` is an absolute path. An absolute path will always
resolve to the same location, regardless of the working directory.

Posix examples:

    path.isAbsolute('/foo/bar') // true
    path.isAbsolute('/baz/..')  // true
    path.isAbsolute('qux/')     // false
    path.isAbsolute('.')        // false

Windows examples:

    path.isAbsolute('//server')  // true
    path.isAbsolute('C:/foo/..') // true
    path.isAbsolute('bar\\baz')   // false
    path.isAbsolute('.')         // false

## path.relative(from, to)

Solve the relative path from `from` to `to`.

At times we have two absolute paths, and we need to derive the relative
path from one to the other.  This is actually the reverse transform of
`path.resolve`, which means we see that:

    path.resolve(from, path.relative(from, to)) == path.resolve(to)

Examples:

    path.relative('C:\\orandea\\test\\aaa', 'C:\\orandea\\impl\\bbb')
    // returns
    '..\\..\\impl\\bbb'

    path.relative('/data/orandea/test/aaa', '/data/orandea/impl/bbb')
    // returns
    '../../impl/bbb'

## path.dirname(p)

Return the directory name of a path.  Similar to the Unix `dirname` command.

Example:

    path.dirname('/foo/bar/baz/asdf/quux')
    // returns
    '/foo/bar/baz/asdf'

## path.basename(p[, ext])

Return the last portion of a path.  Similar to the Unix `basename` command.

Example:

    path.basename('/foo/bar/baz/asdf/quux.html')
    // returns
    'quux.html'

    path.basename('/foo/bar/baz/asdf/quux.html', '.html')
    // returns
    'quux'

## path.extname(p)

Return the extension of the path, from the last '.' to end of string
in the last portion of the path.  If there is no '.' in the last portion
of the path or the first character of it is '.', then it returns
an empty string.  Examples:

    path.extname('index.html')
    // returns
    '.html'

    path.extname('index.coffee.md')
    // returns
    '.md'

    path.extname('index.')
    // returns
    '.'

    path.extname('index')
    // returns
    ''

## path.sep

The platform-specific file separator. `'\\'` or `'/'`.

An example on *nix:

    'foo/bar/baz'.split(path.sep)
    // returns
    ['foo', 'bar', 'baz']

An example on Windows:

    'foo\\bar\\baz'.split(path.sep)
    // returns
    ['foo', 'bar', 'baz']

## path.delimiter

The platform-specific path delimiter, `;` or `':'`.

An example on *nix:

    console.log(process.env.PATH)
    // '/usr/bin:/bin:/usr/sbin:/sbin:/usr/local/bin'

    process.env.PATH.split(path.delimiter)
    // returns
    ['/usr/bin', '/bin', '/usr/sbin', '/sbin', '/usr/local/bin']

An example on Windows:

    console.log(process.env.PATH)
    // 'C:\Windows\system32;C:\Windows;C:\Program Files\nodejs\'

    process.env.PATH.split(path.delimiter)
    // returns
    ['C:\Windows\system32', 'C:\Windows', 'C:\Program Files\nodejs\']

## path.parse(pathString)

Returns an object from a path string.

An example on *nix:

    path.parse('/home/user/dir/file.txt')
    // returns
    {
        root : "/",
        dir : "/home/user/dir",
        base : "file.txt",
        ext : ".txt",
        name : "file"
    }

An example on Windows:

    path.parse('C:\\path\\dir\\index.html')
    // returns
    {
        root : "C:\",
        dir : "C:\path\dir",
        base : "index.html",
        ext : ".html",
        name : "index"
    }

## path.format(pathObject)

Returns a path string from an object, the opposite of `path.parse` above.

    path.format({
        root : "/",
        dir : "/home/user/dir",
        base : "file.txt",
        ext : ".txt",
        name : "file"
    })
    // returns
    '/home/user/dir/file.txt'

## path.posix

Provide access to aforementioned `path` methods but always interact in a posix
compatible way.

## path.win32

Provide access to aforementioned `path` methods but always interact in a win32
compatible way.
