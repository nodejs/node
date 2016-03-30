var fs = require('graceful-fs')
var path = require('path')

var mkdirp = require('mkdirp')
var test = require('tap').test
var rimraf = require('rimraf')

var common = require('../common-tap')

var pkg = path.resolve(__dirname, 'run-script')
var cache = path.resolve(pkg, 'cache')
var tmp = path.resolve(pkg, 'tmp')

var opts = { cwd: pkg }

var fullyPopulated = {
  'name': 'runscript',
  'version': '1.2.3',
  'scripts': {
    'start': 'node -e "console.log(process.argv[1] || \'start\')"',
    'prewith-pre': 'node -e "console.log(process.argv[1] || \'pre\')"',
    'with-pre': 'node -e "console.log(process.argv[1] || \'main\')"',
    'with-post': 'node -e "console.log(process.argv[1] || \'main\')"',
    'postwith-post': 'node -e "console.log(process.argv[1] || \'post\')"',
    'prewith-both': 'node -e "console.log(process.argv[1] || \'pre\')"',
    'with-both': 'node -e "console.log(process.argv[1] || \'main\')"',
    'postwith-both': 'node -e "console.log(process.argv[1] || \'post\')"',
    'stop': 'node -e "console.log(process.argv[1] || \'stop\')"',
    'env-vars': 'node -e "console.log(process.env.run_script_foo_var)"',
    'npm-env-vars': 'node -e "console.log(process.env.npm_run_script_foo_var)"',
    'package-env-vars': 'node -e "console.log(process.env.run_script_foo_var)"',
    'prefixed-package-env-vars': 'node -e "console.log(process.env.npm_package_run_script_foo_var)"'
  },
  'run_script_foo_var': 'run_script_test_foo_val'
}

var lifecycleOnly = {
  name: 'scripted',
  version: '1.2.3',
  scripts: {
    'prestart': 'echo prestart'
  }
}

var directOnly = {
  name: 'scripted',
  version: '1.2.3',
  scripts: {
    'whoa': 'echo whoa'
  }
}

var both = {
  name: 'scripted',
  version: '1.2.3',
  scripts: {
    'prestart': 'echo prestart',
    'whoa': 'echo whoa'
  }
}

var preversionOnly = {
  name: 'scripted',
  version: '1.2.3',
  scripts: {
    'preversion': 'echo preversion'
  }
}


function testOutput (t, command, er, code, stdout, stderr) {
  var lines

  if (er)
    throw er

  if (stderr)
    throw new Error('npm ' + command + ' stderr: ' + stderr.toString())

  lines = stdout.trim().split('\n')
  stdout = lines.filter(function (line) {
    return line.trim() !== '' && line[0] !== '>'
  }).join(';')

  t.equal(stdout, command)
  t.end()
}

function writeMetadata (object) {
  fs.writeFileSync(
    path.resolve(pkg, 'package.json'),
    JSON.stringify(object, null, 2) + '\n'
  )
}

function cleanup () {
  rimraf.sync(pkg)
}

test('setup', function (t) {
  cleanup()
  mkdirp.sync(cache)
  mkdirp.sync(tmp)
  writeMetadata(fullyPopulated)
  t.end()
})

test('npm run-script start', function (t) {
  common.npm(['run-script', 'start'], opts, testOutput.bind(null, t, 'start'))
})

test('npm run-script with args', function (t) {
  common.npm(['run-script', 'start', '--', 'stop'], opts, testOutput.bind(null, t, 'stop'))
})

test('npm run-script with args that contain spaces', function (t) {
  common.npm(['run-script', 'start', '--', 'hello world'], opts, testOutput.bind(null, t, 'hello world'))
})

test('npm run-script with args that contain single quotes', function (t) {
  common.npm(['run-script', 'start', '--', 'they"re awesome'], opts, testOutput.bind(null, t, 'they"re awesome'))
})

test('npm run-script with args that contain double quotes', function (t) {
  common.npm(['run-script', 'start', '--', 'what"s "up"?'], opts, testOutput.bind(null, t, 'what"s "up"?'))
})

test('npm run-script with args that contain ticks', function (t) {
  common.npm(['run-script', 'start', '--', 'what\'s \'up\'?'], opts, testOutput.bind(null, t, 'what\'s \'up\'?'))
})

test('npm run-script with pre script', function (t) {
  common.npm(['run-script', 'with-post'], opts, testOutput.bind(null, t, 'main;post'))
})

test('npm run-script with post script', function (t) {
  common.npm(['run-script', 'with-pre'], opts, testOutput.bind(null, t, 'pre;main'))
})

test('npm run-script with both pre and post script', function (t) {
  common.npm(['run-script', 'with-both'], opts, testOutput.bind(null, t, 'pre;main;post'))
})

test('npm run-script with both pre and post script and with args', function (t) {
  common.npm(['run-script', 'with-both', '--', 'an arg'], opts, testOutput.bind(null, t, 'pre;an arg;post'))
})

test('npm run-script explicitly call pre script with arg', function (t) {
  common.npm(['run-script', 'prewith-pre', '--', 'an arg'], opts, testOutput.bind(null, t, 'an arg'))
})

test('npm run-script test', function (t) {
  common.npm(['run-script', 'test'], opts, function (er, code, stdout, stderr) {
    t.ifError(er, 'npm run-script test ran without issue')
    t.notOk(stderr, 'should not generate errors')
    t.end()
  })
})

test('npm run-script env', function (t) {
  common.npm(['run-script', 'env'], opts, function (er, code, stdout, stderr) {
    t.ifError(er, 'using default env script')
    t.notOk(stderr, 'should not generate errors')
    t.ok(stdout.indexOf('npm_config_init_version') > 0, 'expected values in var list')
    t.end()
  })
})

test('npm run-script nonexistent-script', function (t) {
  common.npm(['run-script', 'nonexistent-script'], opts, function (er, code, stdout, stderr) {
    t.ifError(er, 'npm run-script nonexistent-script did not cause npm to explode')
    t.ok(stderr, 'should generate errors')
    t.end()
  })
})

test('npm run-script restart when there isn\'t restart', function (t) {
  common.npm(['run-script', 'restart'], opts, testOutput.bind(null, t, 'stop;start'))
})

test('npm run-script nonexistent-script with --if-present flag', function (t) {
  common.npm(['run-script', '--if-present', 'nonexistent-script'], opts, function (er, code, stdout, stderr) {
    t.ifError(er, 'npm run-script --if-present non-existent-script ran without issue')
    t.notOk(stderr, 'should not generate errors')
    t.end()
  })
})

test('npm run-script env vars accessible', function (t) {
  process.env.run_script_foo_var = 'run_script_test_foo_val'
  common.npm(['run-script', 'env-vars'], {
    cwd: pkg
  }, function (err, code, stdout, stderr) {
    t.ifError(err, 'ran run-script without crashing')
    t.equal(code, 0, 'exited normally')
    t.equal(stderr, '', 'no error output')
    t.match(stdout,
      new RegExp(process.env.run_script_foo_var),
      'script had env access')
    t.end()
  })
})

test('npm run-script package.json vars injected', function (t) {
  common.npm(['run-script', 'package-env-vars'], {
    cwd: pkg
  }, function (err, code, stdout, stderr) {
    t.ifError(err, 'ran run-script without crashing')
    t.equal(code, 0, 'exited normally')
    t.equal(stderr, '', 'no error output')
    t.match(stdout,
      new RegExp(fullyPopulated.run_script_foo_var),
      'script injected package.json value')
    t.end()
  })
})

test('npm run-script package.json vars injected with prefix', function (t) {
  common.npm(['run-script', 'prefixed-package-env-vars'], {
    cwd: pkg
  }, function (err, code, stdout, stderr) {
    t.ifError(err, 'ran run-script without crashing')
    t.equal(code, 0, 'exited normally')
    t.equal(stderr, '', 'no error output')
    t.match(stdout,
      new RegExp(fullyPopulated.run_script_foo_var),
      'script injected npm_package-prefixed package.json value')
    t.end()
  })
})

test('npm run-script env vars stripped npm-prefixed', function (t) {
  process.env.npm_run_script_foo_var = 'run_script_test_foo_val'
  common.npm(['run-script', 'npm-env-vars'], {
    cwd: pkg
  }, function (err, code, stdout, stderr) {
    t.ifError(err, 'ran run-script without crashing')
    t.equal(code, 0, 'exited normally')
    t.equal(stderr, '', 'no error output')
    t.notMatch(stdout,
      new RegExp(process.env.npm_run_script_foo_var),
      'script stripped npm-prefixed env var')
    t.end()
  })
})

test('npm run-script no-params (lifecycle only)', function (t) {
  var expected = [
    'Lifecycle scripts included in scripted:',
    '  prestart',
    '    echo prestart',
    ''
  ].join('\n')

  writeMetadata(lifecycleOnly)

  common.npm(['run-script'], opts, function (err, code, stdout, stderr) {
    t.ifError(err, 'ran run-script without parameters without crashing')
    t.notOk(code, 'npm exited without error code')
    t.notOk(stderr, 'npm printed nothing to stderr')
    t.equal(stdout, expected, 'got expected output')
    t.end()
  })
})

test('npm run-script no-params (preversion only)', function (t) {
  var expected = [
    'Lifecycle scripts included in scripted:',
    '  preversion',
    '    echo preversion',
    ''
  ].join('\n')

  writeMetadata(preversionOnly)

  common.npm(['run-script'], opts, function (err, code, stdout, stderr) {
    t.ifError(err, 'ran run-script without parameters without crashing')
    t.notOk(code, 'npm exited without error code')
    t.notOk(stderr, 'npm printed nothing to stderr')
    t.equal(stdout, expected, 'got expected output')
    t.end()
  })
})

test('npm run-script no-params (direct only)', function (t) {
  var expected = [
    'Scripts available in scripted via `npm run-script`:',
    '  whoa',
    '    echo whoa',
    ''
  ].join('\n')

  writeMetadata(directOnly)

  common.npm(['run-script'], opts, function (err, code, stdout, stderr) {
    t.ifError(err, 'ran run-script without parameters without crashing')
    t.notOk(code, 'npm exited without error code')
    t.notOk(stderr, 'npm printed nothing to stderr')
    t.equal(stdout, expected, 'got expected output')
    t.end()
  })
})

test('npm run-script no-params (direct only)', function (t) {
  var expected = [
    'Lifecycle scripts included in scripted:',
    '  prestart',
    '    echo prestart',
    '',
    'available via `npm run-script`:',
    '  whoa',
    '    echo whoa',
    ''
  ].join('\n')

  writeMetadata(both)

  common.npm(['run-script'], opts, function (err, code, stdout, stderr) {
    t.ifError(err, 'ran run-script without parameters without crashing')
    t.notOk(code, 'npm exited without error code')
    t.notOk(stderr, 'npm printed nothing to stderr')
    t.equal(stdout, expected, 'got expected output')
    t.end()
  })
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})
