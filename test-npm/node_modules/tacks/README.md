## tacks

Generate fixture modules from folders

### USAGE

Generate a fixture from a folder on disk:

```
tacks /path/to/fixture/example > example.js
```

Create and destroy the fixture from your tests:

```
var Tacks = require('tacks')
var Dir = Tacks.Dir
var File = Tacks.File
var Symlink = Tacks.Symlink

// I like my fixture paths to match my test filename:
var fixturepath = path.join(__dirname, path.basename(__filename, '.js'))

var example = require('./example.js')
example.create(fixturepath)
…
example.remove(fixturepath)
```

Or create your own fixture inline:
```
var example = new Tacks(Dir({
  'package.json': File({
    name: 'example',
    version: '1.0.0'
  })
}))
example.create(fixturepath)
…
example.remove(fixturepath)
```

### STATUS

This is very much a "release early" type release.  Still very much in
progress, but being used.

### CLASSES

These are used in the generated code. It's totally legit to write them directly though.

#### Consturctor

```
var fixture = new Tacks(Dir({
  'package.json': File({
    name: 'example',
    version: '1.0.0'
  })
}))
```

Create a new fixture object based on a `Dir` object, see below.

#### Create Fixture On Disk

```
fixture.create('/path/to/fixture')
```

Take the directory and files described by the fixture and create it in `/path/to/fixture`

#### Remove Fixture From Disk

```
fixture.remove('/path/to/fixture')
```

Cleanup a fixture we installed in `/path/to/fixture`.

#### Add Directory

```
var Dir = Tacks.Dir
var mydir = Tacks.Dir(dirspec)
```

Creates a new `Dir` object for consumption by `new Tacks`.  `dirspec` is a
object whose properties are the names of files in a directory and whose
values are either `File` objects, `Dir` objects or `Symlink` objects.

#### Add File

```
var File = Tacks.File
var myfile = Tacks.File(filespec)
```

Creates a new `File` object for use in `Dir` objects. `filespec` can be
either a `String`, a `Buffer` or an `Object`. In the last case, it
will be stringified with `JSON.stringify` before writing it to disk

#### Add Symlink

```
var Symlink = Tacks.Symlink
var mysymlink = Tacks.Symlink(destination)
```

Creates a new `Symlink` object for use in `Dir` objects. `destination` should
either be relative to where the symlink is being created, or absolute relative
to the root of the fixture. That is, `Tacks.Symlink('/')` will create a symlink
pointing at the fixture root.

#### Generate Fixture Object From Directory

```
var loadFromDir = require('tacks/load-from-dir')
var onDisk = loadFromDir('tests/example')
```
The value returned is a `Tacks` object that you can call `create` or
`remove` on. It's also handy for using in tests use compare an in
memory tacks fixture to whatever ended up on disk.

#### Assert Two Fixtures The Same With node-tap

```
var test = require('tap').test
var tacksAreTheSame = require('tacks/tap').areTheSame
test('example', function (t) {
  return tacksAreTheSame(t, actual, expected, 'got the expected results')
})
```
The `tacks/tap` submodule is the start of tap assertions for comparing fixtures.

`areTheSame` creates a subtest, and inside that subtest runs a bunch of
assertions comparing the contents of the two models.  It's smart enough to
consider `tacks` equivalent things equal, eg strings & buffers with the same
content.

Because it creates a subtest, it's async, it returns the subtest (which is
also a promise) so you can either return it yourself and your test will
complete when it does, or do something like:

```
  tacksAreTheSame(t, actual, expected, 'got the expected results').then(t.done)
```

or

```
  tacksAreTheSame(t, actual, expected, 'got the expected results').then(function () {
    … more tests …
    t.done()
  })
```

#### Geneate JavaScript From Directory

```
var generateFromDir = require('tacks/generate-from-dir')
var fixturestr = Tacks.generateFromDir(dir)
```

This is what's used by the commandline– it generates javascript as a string
from a directory on disk.  It works hard to produce something that looks
like it might have been typed by a human– It translates JSON on disk into
object literals.  And it doesn't quote property names in object literals
unless it has to.  It uses single quotes when it can.  It double quotes when
it has to, and escapes when it has no other choice. It includs plain text
as strings concatenated one per line. For everything else it makes Buffer
objects using hex encoded strings as input.

### WANT TO HAVES

These are things I'll do sooner or late myself.

* Include adding a `.mockFs('/tmp/fixture/path/')` function which returns a
  patched version of `fs` that, for attempts to read from `/tmp/fixture/path`
  returns data from the in memory fixture instead of looking at the
  filesystem.  For injection into tested modules with something like
  `require-inject`.

### NICE TO HAVES

I'd love to see these, but I may never get time to do them myself.  If
someone else did them though…

* Having some way to control the formatting of the generated output would be
  nice for folks who don't use `standard`… eg, semicolons, indentation,
  default quoting. The right answer might be to generate AST objects for
  use by an existing formatter. Relatedly, it'd be nice to have some
  standard extension method for the generated sourcecode. Right now I make
  use of it just by concattenating source code.

