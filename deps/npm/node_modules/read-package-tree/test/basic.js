var test = require('tap').test
var rpt = require('../rpt.js')
var path = require('path')
var fs = require('fs')
var archy = require('archy')
var fixtures = path.resolve(__dirname, 'fixtures')
var roots = [ 'root', 'other', 'selflink', 'noname' ]
var cwd = path.resolve(__dirname, '..')

var symlinks = {
  'selflink/node_modules/@scope/z/node_modules/glob':
    '../../../foo/node_modules/glob',
  'other/node_modules/glob':
    '../../root/node_modules/@scope/x/node_modules/glob',
  'linkedroot':
    'root',
  'deep/root':
    '../root',
  'deeproot':
    'deep'
}

function cleanup () {
  Object.keys(symlinks).forEach(function (s) {
    var p = path.resolve(cwd, 'test/fixtures', s)
    try {
      fs.unlinkSync(p)
    } catch (er) {}
  })
}

test('setup symlinks', function (t) {
  cleanup()

  Object.keys(symlinks).forEach(function (s) {
    var p = path.resolve(cwd, 'test/fixtures', s)
    fs.symlinkSync(symlinks [ s ], p, 'dir')
  })

  t.end()
})

roots.forEach(function (root) {
  var dir = path.resolve(fixtures, root)
  var expectedtxt = path.resolve(dir, 'archy.txt')
  var expectedre  = path.resolve(dir, 'archy.re')

  test(root, function (t) {
    rpt(dir, function (er, d) {
      if (er && er.code !== 'ENOENT') throw er

      var actual = archy(archyize(d)).trim()
      // console . log ('----', dir)
      console.log(actual)
      // console . log (require ('util') . inspect (d, {
      //   depth: Infinity
      // }))
      try {
        var expect = fs.readFileSync(expectedtxt, 'utf8').trim()
        t.equal(actual, expect, root + ' tree')
      } catch (e) {
        var expect = new RegExp(fs.readFileSync(expectedre, 'utf8').trim())
        t.like(actual, expect, root + ' tree')
      }
      t.end()
    })
  })
})

test('linkedroot', function (t) {
  var dir = path.resolve(fixtures, 'linkedroot')
  var out = dir + '-archy.txt'
  rpt(dir, function (er, d) {
    if (er && er.code !== 'ENOENT') throw er

    var actual = archy(archyize(d)).trim()
    console.log(actual)
    var expect = fs.readFileSync(out, 'utf8').trim()
    t.equal(actual, expect, 'linkedroot tree')
    t.end()
  })
})

test('deeproot', function (t) {
  var dir = path.resolve(fixtures, 'deeproot/root')
  var out = path.resolve(fixtures, 'deep') + '-archy.txt'
  rpt(dir, function (er, d) {
    if (er && er.code !== 'ENOENT') throw er

    var actual = archy(archyize(d)).trim()
    console.log(actual)
    var expect = fs.readFileSync(out, 'utf8').trim()
    t.equal(actual, expect, 'deeproot tree')
    t.end()
  })
})

test('broken json', function (t) {
  rpt(path.resolve(fixtures, 'bad'), function (er, d) {
    t.ok(d.error, 'Got an error object')
    t.equal(d.error && d.error.code, 'EJSONPARSE')
    t.ok(d, 'Got a tree')
    t.end()
  })
})

test('missing json does not obscure deeper errors', function (t) {
  rpt(path.resolve(fixtures, 'empty'), function (er, d) {
    var error = d.error
    t.ok(error, 'Error reading json of top level')
    t.equal(error && error.code, 'ENOENT')
    var childError = d.children.length===1 && d.children[0].error
    t.ok(childError, 'Error parsing JSON of child node')
    t.equal(childError && childError.code, 'EJSONPARSE')
    t.end()
  })
})
test('missing folder', function (t) {
  rpt(path.resolve(fixtures, 'does-not-exist'), function (er, d) {
    t.ok(er, 'Got an error object')
    t.equal(er && er.code, 'ENOENT')
    t.ok(!d, 'No tree on top level error')
    t.end()
  })
})


function archyize (d, seen) {
  seen = seen || {}
  var path = d.path
  if (d.target) {
    path = d.target.path
  }

  var label = d.package._id ? d.package._id + ' ' :
              d.package.name ? d.package.name + (d.package.version ? '@' + d.package.version : '') + ' ' :
              ''
  label += path.substr(cwd.length + 1)

  if (d . target) {
    return { label: label + ' (symlink)', nodes: [] }
  }

  return {
    label: label,
    nodes: d.children.map(function (kid) {
      return archyize(kid, seen)
    })
  }
}

test('cleanup', function (t) {
  cleanup()
  t.end()
})
