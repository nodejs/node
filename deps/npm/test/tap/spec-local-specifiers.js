'use strict'
var path = require('path')
var test = require('tap').test
var fs = require('fs')
var rimraf = require('rimraf')
var mr = require('npm-registry-mock')
var Tacks = require('tacks')
var File = Tacks.File
var Symlink = Tacks.Symlink
var Dir = Tacks.Dir
var common = require('../common-tap.js')
var isWindows = require('../../lib/utils/is-windows.js')

var basedir = common.pkg
var testdir = path.join(basedir, 'testdir')
var cachedir = path.join(basedir, 'cache')
var globaldir = path.join(basedir, 'global')
var tmpdir = path.join(basedir, 'tmp')

var conf = {
  cwd: testdir,
  env: common.emptyEnv().extend({
    npm_config_cache: cachedir,
    npm_config_tmp: tmpdir,
    npm_config_prefix: globaldir,
    npm_config_registry: common.registry,
    npm_config_loglevel: 'error'
  }),
  stdio: [0, 'pipe', 2]
}
var confE = {
  cwd: testdir,
  env: conf.env,
  stdio: [0, 'pipe', 'pipe']
}

function readJson (file) {
  return JSON.parse(fs.readFileSync(file))
}

function isSymlink (t, file, message) {
  try {
    var info = fs.lstatSync(file)
    t.is(info.isSymbolicLink(), true, message)
  } catch (ex) {
    if (ex.code === 'ENOENT') {
      t.fail(message, {found: null, wanted: 'symlink', compare: 'fs.lstat(' + file + ')'})
    } else {
      t.fail(message, {found: ex, wanted: 'symlink', compare: 'fs.lstat(' + file + ')'})
    }
  }
}

function fileExists (t, file, message) {
  try {
    fs.statSync(file)
    t.pass(message)
  } catch (ex) {
    if (ex.code === 'ENOENT') {
      t.fail(message, {found: null, wanted: 'exists', compare: 'fs.stat(' + file + ')'})
    } else {
      t.fail(message, {found: ex, wanted: 'exists', compare: 'fs.stat(' + file + ')'})
    }
  }
}

function noFileExists (t, file, message) {
  try {
    fs.statSync(file)
    t.fail(message, {found: 'exists', wanted: 'not exists', compare: 'fs.stat(' + file + ')'})
  } catch (ex) {
    if (ex.code === 'ENOENT') {
      t.pass(message)
    } else {
      t.fail(message, {found: ex, wanted: 'not exists', compare: 'fs.stat(' + file + ')'})
    }
  }
}

var server
var testdirContent = {
  node_modules: Dir({}),
  pkga: Dir({
    'package.json': File({
      name: 'pkga',
      version: '1.0.0'
    })
  }),
  pkgb: Dir({
    'package.json': File({
      name: 'pkgb',
      version: '1.0.0'
    })
  }),
  pkgc: Dir({
    'package.json': File({
      name: 'pkgc',
      version: '1.0.0'
    })
  }),
  pkgd: Dir({
    'package.json': File({
      name: 'pkgd',
      version: '1.0.0'
    })
  })
}

var fixture
function setup () {
  fixture = new Tacks(Dir({
    cache: Dir(),
    global: Dir(),
    tmp: Dir(),
    testdir: Dir(testdirContent)
  }))
  cleanup()
  fixture.create(basedir)
}

function cleanup () {
  fixture.remove(basedir)
}

test('setup', function (t) {
  process.nextTick(function () {
    setup()
    mr({port: common.port, throwOnUnmatched: true}, function (err, s) {
      if (err) throw err
      server = s
      t.done()
    })
  })
})

var installOk = []
var slashes = [ 'unix', 'win' ]
slashes.forEach(function (os) {
  var slash = os === 'unix'
    ? function (ss) { return ss.replace(/\\/g, '/') }
    : function (ss) { return ss.replace(/\//g, '\\') }
  installOk.push(os + '-file-abs-f')
  testdirContent[os + '-file-abs-f'] = Dir({
    'package.json': File({
      name: os + '-file-abs-f',
      version: '1.0.0',
      dependencies: {
        pkga: 'file:' + slash(testdir + '/pkga')
      }
    })
  })
  installOk.push(os + '-file-abs-fff')
  testdirContent[os + '-file-abs-fff'] = Dir({
    'package.json': File({
      name: os + '-file-abs-fff',
      version: '1.0.0',
      dependencies: {
        pkga: 'file://' + slash(testdir + '/pkga')
      }
    })
  })
  installOk.push(os + '-file-rel')
  testdirContent[os + '-file-rel'] = Dir({
    'package.json': File({
      name: os + '-file-rel',
      version: '1.0.0',
      dependencies: {
        pkga: 'file:' + slash('../pkga')
      }
    })
  })
  installOk.push(os + '-file-rel-fffff')
  testdirContent[os + '-file-rel-fffff'] = Dir({
    'package.json': File({
      name: os + '-file-rel-fffff',
      version: '1.0.0',
      dependencies: {
        pkga: 'file:' + slash('/////../pkga')
      }
    })
  })
})

testdirContent['win-abs-drive-win'] = Dir({
  'package.json': File({
    name: 'win-abs-drive-win',
    version: '1.0.0',
    dependencies: {
      pkga: 'file:D:\\thing\\pkga'
    }
  })
})

testdirContent['win-abs-drive-unix'] = Dir({
  'package.json': File({
    name: 'win-abs-drive-unix',
    version: '1.0.0',
    dependencies: {
      pkga: 'file://D:/thing/pkga'
    }
  })
})

test('specifiers', function (t) {
  t.plan(installOk.length + 2)
  installOk.forEach(function (mod) {
    t.test(mod, function (t) {
      common.npm(['install', '--dry-run', '--json', 'file:' + mod], conf, function (err, code, stdout) {
        if (err) throw err
        t.is(code, 0, 'command ran ok')
        t.comment(stdout.trim())
        t.done()
      })
    })
  })
  slashes.forEach(function (os) {
    t.test('win-abs-drive-' + os, function (t) {
      common.npm(['install', '--dry-run', '--json', 'file:win-abs-drive-' + os], confE, function (err, code, stdout, stderr) {
        if (err) throw err
        t.not(code, 0, 'command errored ok')
        t.comment(stderr.trim())
        if (isWindows) {
          t.test('verify failure of file-not-found or wrong drive letter on windows')
        } else {
          var result = JSON.parse(stdout)
          t.is(result.error && result.error.code, 'EWINDOWSPATH', 'verify failure due to windows paths not supported on non-Windows')
        }

        t.done()
      })
    })
  })
})
testdirContent['mkdirp'] = Dir({
  'package.json': File({
    name: 'mkdirp',
    version: '9.9.9'
  })
})
testdirContent['example'] = Dir({
  'minimist': Dir({
    'package.json': File({
      name: 'minimist',
      version: '9.9.9'
    })
  })
})
testdirContent['wordwrap'] = Dir({
})
testdirContent['prefer-pkg'] = Dir({
  'package.json': File({
    name: 'perfer-pkg',
    version: '1.0.0',
    dependencies: {
      'mkdirp': 'latest'
    }
  }),
  'latest': Dir({
    'package.json': File({
      name: 'mkdirp',
      version: '9.9.9'
    })
  })
})

test('ambiguity', function (t) {
  t.plan(5)
  t.test('arg: looks like package name, is dir', function (t) {
    common.npm(['install', 'mkdirp', '--dry-run', '--json'], conf, function (err, code, stdout) {
      if (err) throw err
      t.is(code, 0, 'command ran ok')
      const result = JSON.parse(stdout.trim())
      t.like(result, {added: [{name: 'mkdirp', version: '9.9.9'}]}, 'got local dir')
      t.done()
    })
  })
  t.test('arg: looks like package name, is package', function (t) {
    common.npm(['install', 'wordwrap', '--dry-run', '--json'], conf, function (err, code, stdout) {
      if (err) throw err
      t.is(code, 0, 'command ran ok')
      const result = JSON.parse(stdout.trim())
      t.like(result, {added: [{name: 'wordwrap', version: '0.0.2'}]}, 'even with local dir w/o package.json, got global')
      t.done()
    })
  })
  t.test('arg: looks like github repo, is dir', function (t) {
    common.npm(['install', 'example/minimist', '--dry-run', '--json'], conf, function (err, code, stdout) {
      if (err) throw err
      t.is(code, 0, 'command ran ok')
      const result = JSON.parse(stdout.trim())
      t.like(result, {added: [{name: 'minimist', version: '9.9.9'}]}, 'got local dir')
      t.done()
    })
  })
  t.test('package: looks like tag, has dir, use tag', function (t) {
    common.npm(['install', '--dry-run', '--json'], {cwd: path.join(testdir, 'prefer-pkg'), env: conf.env}, function (err, code, stdout) {
      if (err) throw err
      t.is(code, 0, 'command ran ok')
      const result = JSON.parse(stdout.trim())
      t.like(result, {added: [{name: 'mkdirp', version: '0.3.5'}]}, 'got local dir')
      t.done()
    })
  })

  t.test('test ambiguity for github repos')
})
testdirContent['existing-matches'] = Dir({
  'package.json': File({
    name: 'existing-matches',
    version: '1.0.0',
    dependencies: {
      'pkga': 'file:../pkga',
      'pkgb': 'file:' + testdir + '/pkgb',
      'pkgc': 'file:../pkgc',
      'pkgd': 'file:' + testdir + '/pkgd'
    }
  }),
  'node_modules': Dir({
    'pkga': Symlink('../../pkga'),
    'pkgd': Symlink('../../pkgd')
  })
})

test('existing install matches', function (t) {
  t.plan(1)
  // have to make these by hand because tacks doesn't support absolute paths in symlinks
  fs.symlinkSync(testdir + '/pkgb', testdir + '/existing-matches/node_modules/pkgb', 'junction')
  fs.symlinkSync(testdir + '/pkgc', testdir + '/existing-matches/node_modules/pkgc', 'junction')
  t.test('relative symlink counts as match of relative symlink in package.json', function (t) {
    common.npm(['ls', '--json'], {cwd: path.join(testdir, 'existing-matches'), env: conf.env}, function (err, code, stdout) {
      if (err) throw err
      t.is(code, 0, 'command ran ok')
      if (stdout) t.comment(stdout.trim())
      t.test('specifically test that output is valid')
      // relative symlink counts as match of relative symlink in package.json (pkga)
      // relative symlink counts as match of abs symlink in package.json (pkgc)
      // abs symlink counts as match of relative symlink in package.json (pkgb)
      // abs symlink counts as match of abs symlink in package.json (pkgd)
      t.done()
    })
  })
})
var ibdir = testdir + '/install-behavior'
testdirContent['ib-out'] = Dir({
  'package.json': File({
    name: 'ib-out',
    version: '1.0.0',
    dependencies: {
      'minimist': '*'
    }
  })
})

testdirContent['install-behavior'] = Dir({
  'package.json': File({
    name: 'install-behavior',
    version: '1.0.0'
  }),
  'ib-in': Dir({
    'package.json': File({
      name: 'ib-in',
      version: '1.0.0',
      dependencies: {
        'mkdirp': '*'
      }
    })
  }),
  'noext': File(Buffer.from(
    '1f8b08000000000000032b484cce4e4c4fd52f80d07a59c5f9790c540606' +
    '06066626260a20dadccc144c1b1841f86000923334363037343536343732' +
    '633000728c0c80f2d4760836505a5c925804740aa5e640bca200a78708a8' +
    '56ca4bcc4d55b252cacb4fad2851d251502a4b2d2acecccf030a19ea19e8' +
    '1928d5720db41b47c1281805a36014501f00005012007200080000',
    'hex'
  )),
  'tarball-1.0.0.tgz': File(Buffer.from(
    '1f8b08000000000000032b484cce4e4c4fd52f80d07a59c5f9790c540606' +
    '06066626260a20dadccc144c1b1841f8606062a6c060686c606e686a6c68' +
    '666ec26000e480e5a9ed106ca0b4b824b108e8144acd817845014e0f1150' +
    'ad9497989baa64a5040c85a4c49c1c251d05a5b2d4a2e2ccfc3ca0a0a19e' +
    '819e81522dd740bb72148c8251300a4601b50100473dd15800080000',
    'hex'
  )),
  'not-module': Dir({}),
  'test-preinstall': Dir({
    'preinstall.js': File('console.log("CWD:" + process.cwd())'),
    'package.json': File({
      name: 'test-preinstall',
      version: '1.0.0',
      scripts: {
        'preinstall': 'node ' + ibdir + '/test-preinstall/preinstall.js'
      }
    })
  })
})

test('install behavior', function (t) {
  var ibconf = {cwd: ibdir, env: conf.env.extend({npm_config_loglevel: 'silent'}), stdio: conf.stdio}
  t.plan(7)
  t.test('transitive deps for in-larger-module cases', function (t) {
    common.npm(['install', 'file:ib-in'], ibconf, function (err, code, stdout) {
      if (err) throw err
      t.is(code, 0, 'command ran ok')
      if (stdout) t.comment(stdout.trim())
      fileExists(t, ibdir + '/node_modules/mkdirp', 'transitive dep flattened')
      isSymlink(t, ibdir + '/node_modules/ib-in', 'dep is symlink')
      noFileExists(t, ibdir + '/ib-in/node_modules/mkdirp', 'transitive dep not nested')
      t.done()
    })
  })
  t.test('transitive deps for out-of-larger module cases', function (t) {
    common.npm(['install', 'file:../ib-out'], ibconf, function (err, code, stdout) {
      if (err) throw err
      t.is(code, 0, 'command ran ok')
      if (stdout) t.comment(stdout.trim())
      noFileExists(t, ibdir + '/node_modules/minimist', 'transitive dep not flattened')
      fileExists(t, testdir + '/ib-out/node_modules/minimist', 'transitive dep nested')
      t.done()
    })
  })
  t.test('transitive deps for out-of-larger module cases w/ --preserve-symlinks', function (t) {
    rimraf.sync(ibdir + '/node_modules')
    rimraf.sync(testdir + '/ib-out/node_modules')
    var env = conf.env.extend({NODE_PRESERVE_SYMLINKS: '1'})
    common.npm(['install', 'file:../ib-out'], {cwd: ibdir, env: env}, function (err, code, stdout) {
      if (err) throw err
      t.is(code, 0, 'command ran ok')
      if (stdout) t.comment(stdout.trim())
      fileExists(t, ibdir + '/node_modules/minimist', 'transitive dep flattened')
      noFileExists(t, testdir + '/ib-out/node_modules/minimist', 'transitive dep not nested')
      t.done()
    })
  })
  t.test('tar/tgz/tar.gz should install a tarball', function (t) {
    common.npm(['install', 'file:tarball-1.0.0.tgz'], ibconf, function (err, code, stdout) {
      if (err) throw err
      t.is(code, 0, 'command ran ok')
      if (stdout) t.comment(stdout.trim())
      fileExists(t, ibdir + '/node_modules/tarball')
      t.done()
    })
  })
  t.test('non tar/tgz/tar.gz files should give good error message', function (t) {
    common.npm(['install', 'file:noext', '--json'], ibconf, function (err, code, stdout) {
      if (err) throw err
      t.not(code, 0, 'do not install files w/o extensions')
      noFileExists(t, ibdir + '/node_modules/noext')
      var result = JSON.parse(stdout)
      t.like(result, {error: {code: 'ENOLOCAL'}}, 'new type of error')
      t.done()
    })
  })
  t.test('directories without package.json should give good error message', function (t) {
    common.npm(['install', 'file:not-module', '--json'], ibconf, function (err, code, stdout) {
      if (err) throw err
      t.not(code, 0, 'error on dir w/o module')
      var result = JSON.parse(stdout)
      t.like(result, {error: {code: 'ENOLOCAL'}}, 'new type of error')
      t.done()
    })
  })
  t.test('verify preinstall step runs after finalize, such that cwd is as expected', function (t) {
    common.npm(['install', 'file:test-preinstall'], ibconf, function (err, code, stdout) {
      if (err) throw err
      t.is(code, 0, 'command ran ok')
      isSymlink(t, `${ibdir}/node_modules/test-preinstall`, 'installed as symlink')
      t.notLike(stdout, /CWD:.*[.]staging/, 'cwd should not be in staging')

      t.test('verify that env is as expected')
      t.done()
    })
  })
})
var sbdir = testdir + '/save-behavior'
testdirContent['save-behavior'] = Dir({
  'package.json': File({
    name: 'save-behavior',
    version: '1.0.0'
  }),
  'npm-shrinkwrap.json': File({
    name: 'save-behavior',
    version: '1.0.0',
    dependencies: {}
  }),
  'transitive': Dir({
    'package.json': File({
      name: 'transitive',
      version: '1.0.0',
      dependencies: {
        'pkgc': 'file:../../pkgc'
      }
    })
  })
})
testdirContent['sb-transitive'] = Dir({
  'package.json': File({
    name: 'sb-transitive',
    version: '1.0.0',
    dependencies: {
      'sbta': 'file:sbta'
    }
  }),
  'sbta': Dir({
    'package.json': File({
      name: 'sbta',
      version: '1.0.0'
    })
  })
})
var sbdirp = testdir + '/save-behavior-pre'
testdirContent['save-behavior-pre'] = Dir({
  'package.json': File({
    name: 'save-behavior',
    version: '1.0.0'
  }),
  'npm-shrinkwrap.json': File({
    name: 'save-behavior',
    version: '1.0.0',
    dependencies: {}
  })
})
testdirContent['sb-transitive-preserve'] = Dir({
  'package.json': File({
    name: 'sb-transitive-preserve',
    version: '1.0.0',
    dependencies: {
      'sbtb': 'file:sbtb'
    }
  }),
  'sbtb': Dir({
    'package.json': File({
      name: 'sbtb',
      version: '1.0.0'
    })
  })
})
test('save behavior', function (t) {
  t.plan(6)
  var sbconf = {cwd: sbdir, env: conf.env, stdio: conf.stdio}
  t.test('to package.json and npm-shrinkwrap.json w/ abs', function (t) {
    common.npm(['install', '--save', 'file:' + testdir + '/pkga'], sbconf, function (err, code, stdout) {
      if (err) throw err
      t.is(code, 0, 'command ran ok')
      var pjson = readJson(sbdir + '/package.json')
      var shrinkwrap = readJson(sbdir + '/npm-shrinkwrap.json')
      t.is(pjson.dependencies.pkga, 'file:../pkga', 'package.json')
      var sdep = shrinkwrap.dependencies
      t.like(sdep, {pkga: {version: 'file:../pkga', resolved: null}}, 'npm-shrinkwrap.json')
      t.done()
    })
  })
  t.test('to package.json and npm-shrinkwrap.json w/ drive abs')
  t.test('to package.json and npm-shrinkwrap.json w/ rel', function (t) {
    common.npm(['install', '--save', 'file:../pkgb'], sbconf, function (err, code, stdout) {
      if (err) throw err
      t.is(code, 0, 'command ran ok')
      var pjson = readJson(sbdir + '/package.json')
      var shrinkwrap = readJson(sbdir + '/npm-shrinkwrap.json')
      t.is(pjson.dependencies.pkgb, 'file:../pkgb', 'package.json')
      var sdep = shrinkwrap.dependencies
      t.like(sdep, {pkgb: {version: 'file:../pkgb', resolved: null}}, 'npm-shrinkwrap.json')
      t.done()
    })
  })
  t.test('internal transitive dependencies of shrinkwraps', function (t) {
    common.npm(['install', '--save', 'file:transitive'], sbconf, function (err, code, stdout) {
      if (err) throw err
      t.is(code, 0, 'command ran ok')
      var pjson = readJson(sbdir + '/package.json')
      var shrinkwrap = readJson(sbdir + '/npm-shrinkwrap.json')
      t.is(pjson.dependencies.transitive, 'file:transitive', 'package.json')
      var sdep = shrinkwrap.dependencies.transitive || {}
      var tdep = shrinkwrap.dependencies.pkgc
      t.is(sdep.version, 'file:transitive', 'npm-shrinkwrap.json direct dep')
      t.is(tdep.version, 'file:../pkgc', 'npm-shrinkwrap.json transitive dep')
      t.done()
    })
  })
  t.test('external transitive dependencies of shrinkwraps', function (t) {
    common.npm(['install', '--save', 'file:../sb-transitive'], sbconf, function (err, code, stdout) {
      if (err) throw err
      t.is(code, 0, 'command ran ok')
      var pjson = readJson(sbdir + '/package.json')
      var shrinkwrap = readJson(sbdir + '/npm-shrinkwrap.json')
      var deps = pjson.dependencies || {}
      t.is(deps['sb-transitive'], 'file:../sb-transitive', 'package.json')
      var sdep = shrinkwrap.dependencies['sb-transitive'] || {}
      t.like(sdep, {bundled: null, dependencies: null, version: 'file:../sb-transitive'}, 'npm-shrinkwrap.json direct dep')
      t.done()
    })
  })
  t.test('external transitive dependencies of shrinkwraps > preserve symlinks', function (t) {
    var preserveEnv = conf.env.extend({NODE_PRESERVE_SYMLINKS: '1'})
    var preserveConf = {cwd: sbdirp, env: preserveEnv, stdio: conf.stdio}
    common.npm(['install', '--save', 'file:../sb-transitive-preserve'], preserveConf, function (err, code, stdout) {
      if (err) throw err
      t.is(code, 0, 'command ran ok')
      var pjson = readJson(sbdirp + '/package.json')
      var shrinkwrap = readJson(sbdirp + '/npm-shrinkwrap.json')
      t.is(pjson.dependencies['sb-transitive-preserve'], 'file:../sb-transitive-preserve', 'package.json')
      var sdep = shrinkwrap.dependencies['sb-transitive-preserve'] || {}
      var tdep = shrinkwrap.dependencies.sbtb
      t.like(sdep, {bundled: null, version: 'file:../sb-transitive-preserve'}, 'npm-shrinkwrap.json direct dep')
      t.like(tdep, {bundled: null, version: 'file:../sb-transitive-preserve/sbtb'}, 'npm-shrinkwrap.json transitive dep')
      t.done()
    })
  })
})

var rmdir = testdir + '/remove-behavior'
testdirContent['remove-behavior'] = Dir({
  'rmsymlink': Dir({
    'package.json': File({
      name: 'remove-behavior',
      version: '1.0.0',
      dependencies: {
        dep1: 'file:dep1'
      }
    }),
    'package-lock.json': File({
      name: 'remove-behavior',
      version: '1.0.0',
      lockfileVersion: 1,
      requires: true,
      dependencies: {
        dep1: {
          version: 'file:dep1',
          requires: {
            dep2: 'file:dep2'
          },
          dependencies: {
            dep2: {
              version: 'file:dep2',
              bundled: true
            }
          }
        }
      }
    }),
    dep1: Dir({
      'package.json': File({
        name: 'dep1',
        version: '1.0.0',
        dependencies: {
          dep2: 'file:../dep2'
        }
      }),
      'node_modules': Dir({
        dep2: Symlink('../../dep2')
      })
    }),
    dep2: Dir({
      'package.json': File({
        name: 'dep2',
        version: '1.0.0'
      })
    }),
    'node_modules': Dir({
      dep1: Symlink('../dep1')
    })
  }),
  'rmesymlink': Dir({
    'package.json': File({
      name: 'remove-behavior',
      version: '1.0.0',
      dependencies: {
        edep1: 'file:../edep1'
      }
    }),
    'package-lock.json': File({
      name: 'remove-behavior',
      version: '1.0.0',
      lockfileVersion: 1,
      requires: true,
      dependencies: {
        edep1: {
          version: 'file:../edep1',
          requires: {
            edep2: 'file:../edep2'
          },
          dependencies: {
            edep2: {
              version: 'file:../edep2',
              bundled: true
            }
          }
        }
      }
    }),
    'node_modules': Dir({
      edep1: Symlink('../../edep1')
    })
  }),
  edep1: Dir({
    'package.json': File({
      name: 'edep1',
      version: '1.0.0',
      dependencies: {
        edep2: 'file:../edep2'
      }
    }),
    'node_modules': Dir({
      edep2: Symlink('../../edep2')
    })
  }),
  edep2: Dir({
    'package.json': File({
      name: 'edep2',
      version: '1.0.0'
    })
  })
})

test('removal', function (t) {
  t.plan(2)

  t.test('should remove the symlink', (t) => {
    const rmconf = {cwd: `${rmdir}/rmsymlink`, env: conf.env, stdio: conf.stdio}
    return common.npm(['uninstall', 'dep1'], rmconf).spread((code, stdout) => {
      t.is(code, 0, 'uninstall ran ok')
      t.comment(stdout)
      noFileExists(t, `${rmdir}/rmsymlink/node_modules/dep1`, 'removed symlink')
      noFileExists(t, `${rmdir}/rmsymlink/dep1/node_modules/dep2`, 'removed transitive dep')
      fileExists(t, `${rmdir}/rmsymlink/dep2`, 'original transitive dep still exists')
    })
  })
  t.test("should not remove transitive deps if it's outside the package and --preserver-symlinks isn't set", (t) => {
    const rmconf = {cwd: `${rmdir}/rmesymlink`, env: conf.env, stdio: conf.stdio}
    return common.npm(['uninstall', 'edep1'], rmconf).spread((code, stdout) => {
      t.is(code, 0, 'uninstall ran ok')
      t.comment(stdout)
      noFileExists(t, `${rmdir}/rmsymlink/node_modules/edep1`, 'removed symlink')
      fileExists(t, `${rmdir}/edep1/node_modules/edep2`, 'did NOT remove transitive dep')
      fileExists(t, `${rmdir}/edep2`, 'original transitive dep still exists')
    })
  })
})

test('misc', function (t) {
  t.plan(3)
  t.test('listing: should look right, not include version')
  t.test('outdated: show LOCAL for wanted / latest')
  t.test('update: if specifier exists, do nothing. otherwise as if `npm install`.')
})

test('cleanup', function (t) {
  server.close()
  cleanup()
  t.done()
})
