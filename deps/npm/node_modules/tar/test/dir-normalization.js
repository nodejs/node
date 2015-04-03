// Set the umask, so that it works the same everywhere.
process.umask(parseInt('22', 8))

var fs = require('fs')
var path = require('path')

var fstream = require('fstream')
var test = require('tap').test

var tar = require('../tar.js')
var file = path.resolve(__dirname, 'dir-normalization.tar')
var target = path.resolve(__dirname, 'tmp/dir-normalization-test')
var ee = 0

var expectEntries = [
  { path: 'fixtures/',
    mode: '755',
    type: '5',
    depth: undefined,
    size: 0,
    linkpath: '',
    nlink: undefined,
    dev: undefined,
    ino: undefined
  },
  { path: 'fixtures/the-chumbler',
    mode: '755',
    type: '2',
    depth: undefined,
    size: 0,
    linkpath: path.resolve(target, 'a/b/c/d/the-chumbler'),
    nlink: undefined,
    dev: undefined,
    ino: undefined
  }
]

var ef = 0
var expectFiles = [
  { path: '',
    mode: '40755',
    type: 'Directory',
    depth: 0,
    linkpath: undefined
  },
  { path: '/fixtures',
    mode: '40755',
    type: 'Directory',
    depth: 1,
    linkpath: undefined
  },
  { path: '/fixtures/the-chumbler',
    mode: '120755',
    type: 'SymbolicLink',
    depth: 2,
    size: 95,
    linkpath: path.resolve(target, 'a/b/c/d/the-chumbler'),
    nlink: 1
  }
]

test('preclean', function (t) {
  require('rimraf').sync(path.join(__dirname, '/tmp/dir-normalization-test'))
  t.pass('cleaned!')
  t.end()
})

test('extract test', function (t) {
  var extract = tar.Extract(target)
  var inp = fs.createReadStream(file)

  inp.pipe(extract)

  extract.on('end', function () {
    t.equal(ee, expectEntries.length, 'should see ' + expectEntries.length + ' entries')

    // should get no more entries after end
    extract.removeAllListeners('entry')
    extract.on('entry', function (e) {
      t.fail('Should not get entries after end!')
    })

    next()
  })

  extract.on('entry', function (entry) {
    var found = {
      path: entry.path,
      mode: entry.props.mode.toString(8),
      type: entry.props.type,
      depth: entry.props.depth,
      size: entry.props.size,
      linkpath: entry.props.linkpath,
      nlink: entry.props.nlink,
      dev: entry.props.dev,
      ino: entry.props.ino
    }

    var wanted = expectEntries[ee++]

    t.equivalent(found, wanted, 'tar entry ' + ee + ' ' + wanted.path)
  })

  function next () {
    var r = fstream.Reader({
      path: target,
      type: 'Directory',
      sort: 'alpha'
    })

    r.on('ready', function () {
      foundEntry(r)
    })

    r.on('end', finish)

    function foundEntry (entry) {
      var p = entry.path.substr(target.length)
      var found = {
        path: p,
        mode: entry.props.mode.toString(8),
        type: entry.props.type,
        depth: entry.props.depth,
        size: entry.props.size,
        linkpath: entry.props.linkpath,
        nlink: entry.props.nlink
      }

      var wanted = expectFiles[ef++]

      t.has(found, wanted, 'unpacked file ' + ef + ' ' + wanted.path)

      entry.on('entry', foundEntry)
    }

    function finish () {
      t.equal(ef, expectFiles.length, 'should have ' + ef + ' items')
      t.end()
    }
  }
})
