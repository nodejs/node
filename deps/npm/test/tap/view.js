var common = require('../common-tap.js')
var test = require('tap').test
var osenv = require('osenv')
var path = require('path')
var fs = require('fs')
var rimraf = require('rimraf')
var mkdirp = require('mkdirp')
var tmp = osenv.tmpdir()
var t1dir = path.resolve(tmp, 'view-local-no-pkg')
var t2dir = path.resolve(tmp, 'view-local-notmine')
var t3dir = path.resolve(tmp, 'view-local-mine')
var mr = require('npm-registry-mock')

var server

test('setup', function (t) {
  mkdirp.sync(t1dir)
  mkdirp.sync(t2dir)
  mkdirp.sync(t3dir)

  fs.writeFileSync(t2dir + '/package.json', JSON.stringify({
    author: 'Evan Lucas',
    name: 'test-repo-url-https',
    version: '0.0.1'
  }), 'utf8')

  fs.writeFileSync(t3dir + '/package.json', JSON.stringify({
    author: 'Evan Lucas',
    name: 'biscuits',
    version: '0.0.1'
  }), 'utf8')

  t.pass('created fixtures')

  mr({ port: common.port, plugin: plugin }, function (er, s) {
    server = s
    t.end()
  })
})

function plugin (server) {
  server
    .get('/biscuits')
    .many()
    .reply(404, {'error': 'version not found'})
}

test('npm view . in global mode', function (t) {
  process.chdir(t1dir)
  common.npm([
    'view',
    '.',
    '--registry=' + common.registry,
    '--global'
  ], { cwd: t1dir }, function (err, code, stdout, stderr) {
    t.ifError(err, 'view command finished successfully')
    t.equal(code, 1, 'exit not ok')
    t.similar(stderr, /Cannot use view command in global mode./m)
    t.end()
  })
})

test('npm view --global', function (t) {
  process.chdir(t1dir)
  common.npm([
    'view',
    '--registry=' + common.registry,
    '--global'
  ], { cwd: t1dir }, function (err, code, stdout, stderr) {
    t.ifError(err, 'view command finished successfully')
    t.equal(code, 1, 'exit not ok')
    t.similar(stderr, /Cannot use view command in global mode./m)
    t.end()
  })
})

test('npm view . with no package.json', function (t) {
  process.chdir(t1dir)
  common.npm([
    'view',
    '.',
    '--registry=' + common.registry
  ], { cwd: t1dir }, function (err, code, stdout, stderr) {
    t.ifError(err, 'view command finished successfully')
    t.equal(code, 1, 'exit not ok')
    t.similar(stderr, /Invalid package.json/m)
    t.end()
  })
})

test('npm view . with no published package', function (t) {
  process.chdir(t3dir)
  common.npm([
    'view',
    '.',
    '--registry=' + common.registry
  ], { cwd: t3dir }, function (err, code, stdout, stderr) {
    t.ifError(err, 'view command finished successfully')
    t.equal(code, 1, 'exit not ok')
    t.similar(stderr, /version not found/m)
    t.end()
  })
})

test('npm view .', function (t) {
  process.chdir(t2dir)
  common.npm([
    'view',
    '.',
    '--registry=' + common.registry
  ], { cwd: t2dir }, function (err, code, stdout) {
    t.ifError(err, 'view command finished successfully')
    t.equal(code, 0, 'exit ok')
    var re = new RegExp("name: 'test-repo-url-https'")
    t.similar(stdout, re)
    t.end()
  })
})

test('npm view . select fields', function (t) {
  process.chdir(t2dir)
  common.npm([
    'view',
    '.',
    'main',
    '--registry=' + common.registry
  ], { cwd: t2dir }, function (err, code, stdout) {
    t.ifError(err, 'view command finished successfully')
    t.equal(code, 0, 'exit ok')
    t.equal(stdout.trim(), 'index.js', 'should print `index.js`')
    t.end()
  })
})

test('npm view .@<version>', function (t) {
  process.chdir(t2dir)
  common.npm([
    'view',
    '.@0.0.0',
    'version',
    '--registry=' + common.registry
  ], { cwd: t2dir }, function (err, code, stdout) {
    t.ifError(err, 'view command finished successfully')
    t.equal(code, 0, 'exit ok')
    t.equal(stdout.trim(), '0.0.0', 'should print `0.0.0`')
    t.end()
  })
})

test('npm view .@<version> version --json', function (t) {
  process.chdir(t2dir)
  common.npm([
    'view',
    '.@0.0.0',
    'version',
    '--json',
    '--registry=' + common.registry
  ], { cwd: t2dir }, function (err, code, stdout) {
    t.ifError(err, 'view command finished successfully')
    t.equal(code, 0, 'exit ok')
    t.equal(stdout.trim(), '"0.0.0"', 'should print `"0.0.0"`')
    t.end()
  })
})

test('npm view . --json author name version', function (t) {
  process.chdir(t2dir)
  common.npm([
    'view',
    '.',
    'author',
    'name',
    'version',
    '--json',
    '--registry=' + common.registry
  ], { cwd: t2dir }, function (err, code, stdout) {
    var expected = JSON.stringify({
      author: 'Evan Lucas <evanlucas@me.com>',
      name: 'test-repo-url-https',
      version: '0.0.1'
    }, null, 2)
    t.ifError(err, 'view command finished successfully')
    t.equal(code, 0, 'exit ok')
    t.equal(stdout.trim(), expected, 'should print ' + expected)
    t.end()
  })
})

test('npm view .@<version> --json author name version', function (t) {
  process.chdir(t2dir)
  common.npm([
    'view',
    '.@0.0.0',
    'author',
    'name',
    'version',
    '--json',
    '--registry=' + common.registry
  ], { cwd: t2dir }, function (err, code, stdout) {
    var expected = JSON.stringify({
      author: 'Evan Lucas <evanlucas@me.com>',
      name: 'test-repo-url-https',
      version: '0.0.0'
    }, null, 2)
    t.ifError(err, 'view command finished successfully')
    t.equal(code, 0, 'exit ok')
    t.equal(stdout.trim(), expected, 'should print ' + expected)
    t.end()
  })
})

test('npm view <package name>', function (t) {
  common.npm([
    'view',
    'underscore',
    '--registry=' + common.registry
  ], { cwd: t2dir }, function (err, code, stdout) {
    t.ifError(err, 'view command finished successfully')
    t.equal(code, 0, 'exit ok')
    var re = new RegExp("name: 'underscore'")
    t.similar(stdout, re, 'should have name `underscore`')
    t.end()
  })
})

test('npm view <package name> --global', function (t) {
  common.npm([
    'view',
    'underscore',
    '--global',
    '--registry=' + common.registry
  ], { cwd: t2dir }, function (err, code, stdout) {
    t.ifError(err, 'view command finished successfully')
    t.equal(code, 0, 'exit ok')
    var re = new RegExp("name: 'underscore'")
    t.similar(stdout, re, 'should have name `underscore`')
    t.end()
  })
})

test('npm view <package name>@<semver range> versions', function (t) {
  common.npm([
    'view',
    'underscore@^1.5.0',
    'versions',
    '--registry=' + common.registry
  ], { cwd: t2dir }, function (err, code, stdout) {
    t.ifError(err, 'view command finished successfully')
    t.equal(code, 0, 'exit ok')
    var re = new RegExp('1.5.0')
    t.similar(stdout, re, 'should have version `1.5.0`')
    t.end()
  })
})

test('npm view <package name>@<semver range> version --json', function (t) {
  common.npm([
    'view',
    'underscore@~1.5.0',
    'version',
    '--json',
    '--registry=' + common.registry
  ], { cwd: t2dir }, function (err, code, stdout) {
    t.ifError(err, 'view command finished successfully')
    t.equal(code, 0, 'exit ok')
    t.equal(stdout.trim(), JSON.stringify([
      '1.5.0',
      '1.5.1'
    ], null, 2), 'should have three versions')
    t.end()
  })
})

test('npm view <package name> --json', function (t) {
  t.plan(3)
  common.npm([
    'view',
    'underscore',
    '--json',
    '--registry=' + common.registry
  ], { cwd: t2dir }, function (err, code, stdout) {
    t.ifError(err, 'view command finished successfully')
    t.equal(code, 0, 'exit ok')
    try {
      var out = JSON.parse(stdout.trim())
      t.similar(out, {
        maintainers: ['jashkenas <jashkenas@gmail.com>']
      }, 'should have the same maintainer')
    } catch (er) {
      t.fail('Unable to parse JSON')
    }
  })
})

test('npm view <package name>@<invalid version>', function (t) {
  common.npm([
    'view',
    'underscore@12345',
    '--registry=' + common.registry
  ], { cwd: t2dir }, function (err, code, stdout) {
    t.ifError(err, 'view command finished successfully')
    t.equal(code, 0, 'exit ok')
    t.equal(stdout.trim(), '', 'should return empty')
    t.end()
  })
})

test('npm view <package name>@<invalid version> --json', function (t) {
  common.npm([
    'view',
    'underscore@12345',
    '--json',
    '--registry=' + common.registry
  ], { cwd: t2dir }, function (err, code, stdout) {
    t.ifError(err, 'view command finished successfully')
    t.equal(code, 0, 'exit ok')
    t.equal(stdout.trim(), '', 'should return empty')
    t.end()
  })
})

test('npm view <package name> <field>', function (t) {
  common.npm([
    'view',
    'underscore',
    'homepage',
    '--registry=' + common.registry
  ], { cwd: t2dir }, function (err, code, stdout) {
    t.ifError(err, 'view command finished successfully')
    t.equal(code, 0, 'exit ok')
    t.equal(stdout.trim(), 'http://underscorejs.org',
      'homepage should equal `http://underscorejs.org`')
    t.end()
  })
})

test('npm view with invalid package name', function (t) {
  var invalidName = 'InvalidPackage'

  server.get('/' + invalidName).reply('404', {'error': 'not found'})
  common.npm([
    'view',
    invalidName,
    '--registry=' + common.registry
  ], {}, function (err, code, stdout, stderr) {
    t.ifError(err, 'view command finished successfully')
    t.equal(code, 1, 'exit not ok')

    t.similar(stderr, new RegExp('is not in the npm registry'),
      'Package should NOT be found')

    t.dissimilar(stderr, new RegExp('use the name yourself!'),
      'Suggestion should not be there')

    t.similar(stderr, new RegExp('name can no longer contain capital letters'),
      'Suggestion about Capital letter should be there')

    t.end()
  })
})

test('npm view with valid but non existent package name', function (t) {
  server.get('/valid-but-non-existent-package').reply(404, {'error': 'not found'})
  common.npm([
    'view',
    'valid-but-non-existent-package',
    '--registry=' + common.registry
  ], {}, function (err, code, stdout, stderr) {
    t.ifError(err, 'view command finished successfully')
    t.equal(code, 1, 'exit not ok')

    t.similar(stderr,
      new RegExp("'valid-but-non-existent-package' is not in the npm registry\."),
      'Package should NOT be found')

    t.similar(stderr, new RegExp('use the name yourself!'),
      'Suggestion should be there')

    t.end()
  })
})

test('cleanup', function (t) {
  process.chdir(osenv.tmpdir())
  rimraf.sync(t1dir)
  rimraf.sync(t2dir)
  rimraf.sync(t3dir)
  t.pass('cleaned up')
  server.close()
  t.end()
})
