## Path

This module contains utilities for dealing with file paths.  Use
`require('path')` to use it.  It provides the following methods:

### path.normalize(p)

Normalize a string path, taking care of `'..'` and `'.'` parts.

When multiple slashes are found, they're replaces by a single one;
when the path contains a trailing slash, it is preserved.
On windows backslashes are used. 

Example:

    path.normalize('/foo/bar//baz/asdf/quux/..')
    // returns
    '/foo/bar/baz/asdf'

### path.join([path1], [path2], [...])

Join all arguments together and normalize the resulting path.

Example:

    node> require('path').join(
    ...   '/foo', 'bar', 'baz/asdf', 'quux', '..')
    '/foo/bar/baz/asdf'

### path.resolve([from ...], to)

Resolves `to` to an absolute path.

If `to` isn't already absolute `from` arguments are prepended in right to left
order, until an absolute path is found. If after using all `from` paths still
no absolute path is found, the current working directory is used as well. The
resulting path is normalized, and trailing slashes are removed unless the path 
gets resolved to the root directory.

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

### path.dirname(p)

Return the directory name of a path.  Similar to the Unix `dirname` command.

Example:

    path.dirname('/foo/bar/baz/asdf/quux')
    // returns
    '/foo/bar/baz/asdf'

### path.basename(p, [ext])

Return the last portion of a path.  Similar to the Unix `basename` command.

Example:

    path.basename('/foo/bar/baz/asdf/quux.html')
    // returns
    'quux.html'

    path.basename('/foo/bar/baz/asdf/quux.html', '.html')
    // returns
    'quux'

### path.extname(p)

Return the extension of the path.  Everything after the last '.' in the last portion
of the path.  If there is no '.' in the last portion of the path or the only '.' is
the first character, then it returns an empty string.  Examples:

    path.extname('index.html')
    // returns
    '.html'

    path.extname('index')
    // returns
    ''

### path.exists(p, [callback])

Test whether or not the given path exists.  Then, call the `callback` argument
with either true or false. Example:

    path.exists('/etc/passwd', function (exists) {
      util.debug(exists ? "it's there" : "no passwd!");
    });


### path.existsSync(p)

Synchronous version of `path.exists`.
