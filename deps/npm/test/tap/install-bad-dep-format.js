var fs = require('graceful-fs')
var path = require('path')

var mkdirp = require('mkdirp')
var test = require('tap').test

var common = require('../common-tap.js')

var json = {
  author: 'John Foo',
  name: 'bad-dep-format',
  version: '0.0.0',
  dependencies: {
    'not-legit': 'bad:not-legit@1.0'
  }
}

test('invalid url format returns appropriate error', function (t) {
  var pkgPath = path.resolve(common.pkg, json.name)
  mkdirp.sync(pkgPath)
  fs.writeFileSync(
    path.join(pkgPath, 'package.json'),
    JSON.stringify(json, null, 2)
  )
  common.npm(['install'], {cwd: pkgPath}, function (err, code, stdout, stderr) {
    t.ifError(err, 'install ran without error')
    t.equals(code, 1, 'install exited with code 1')
    t.match(stderr,
      /ERR.*Unsupported URL Type/,
      'Error should report that invalid url-style formats are used')
    t.end()
  })
})
