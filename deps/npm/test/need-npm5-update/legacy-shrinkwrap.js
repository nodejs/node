'use strict'
var test = require('tap').test
var common = require('../common-tap.js')
var path = require('path')
var basepath = path.resolve(__dirname, path.basename(__filename, '.js'))
var Tacks = require('tacks')
var File = Tacks.File
var Dir = Tacks.Dir

var fixture = new Tacks(
  Dir({
    README: File(
      'just an npm test\n'
    ),
    'npm-shrinkwrap.json': File({
      name: 'npm-test-shrinkwrap',
      version: '0.0.0',
      dependencies: {
        glob: {
          version: '3.1.5',
          from: 'git://github.com/isaacs/node-glob.git#npm-test',
          resolved: 'git://github.com/isaacs/node-glob.git#67bda227fd7a559cca5620307c7d30a6732a792f',
          dependencies: {
            'graceful-fs': {
              version: '1.1.5',
              resolved: 'https://registry.npmjs.org/graceful-fs/-/graceful-fs-1.1.5.tgz',
              dependencies: {
                'fast-list': {
                  version: '1.0.2',
                  resolved: 'https://registry.npmjs.org/fast-list/-/fast-list-1.0.2.tgz'
                }
              }
            },
            inherits: {
              version: '1.0.0',
              resolved: 'https://registry.npmjs.org/inherits/-/inherits-1.0.0.tgz'
            },
            minimatch: {
              version: '0.2.1',
              dependencies: {
                'lru-cache': {
                  version: '1.0.5'
                }
              }
            }
          }
        },
        minimatch: {
          version: '0.1.5',
          resolved: 'https://registry.npmjs.org/minimatch/-/minimatch-0.1.5.tgz',
          dependencies: {
            'lru-cache': {
              version: '1.0.5',
              resolved: 'https://registry.npmjs.org/lru-cache/-/lru-cache-1.0.5.tgz'
            }
          }
        },
        'npm-test-single-file': {
          version: '1.2.3',
          resolved: 'https://gist.github.com/isaacs/1837112/raw/9ef57a59fc22aeb1d1ca346b68826dcb638b8416/index.js'
        }
      }
    }),
    'package.json': File({
      author: 'Isaac Z. Schlueter <i@izs.me> (http://blog.izs.me/)',
      name: 'npm-test-shrinkwrap',
      version: '0.0.0',
      dependencies: {
        'npm-test-single-file': 'https://gist.github.com/isaacs/1837112/raw/9ef57a59fc22aeb1d1ca346b68826dcb638b8416/index.js',
        glob: 'git://github.com/isaacs/node-glob.git#npm-test',
        minimatch: '~0.1.0'
      },
      scripts: {
        test: 'node test.js'
      }
    })
  })
)

test('setup', function (t) {
  setup()
  t.done()
})

test('shrinkwrap', function (t) {
  common.npm(['install'], {cwd: basepath}, installCheckAndTest)

  function installCheckAndTest (err, code, stdout, stderr) {
    if (err) throw err
    console.error(stderr)
    t.is(code, 0, 'install went ok')

    common.npm(['ls', '--json'], {cwd: basepath}, verifyLsMatchesShrinkwrap)
  }

  function verifyLsMatchesShrinkwrap (err, code, stdout, stderr) {
    if (err) throw err
    console.error(stderr)
    t.is(code, 0, 'ls went ok')
    var actual = JSON.parse(stdout)
    var expected = require(path.resolve(basepath, 'npm-shrinkwrap.json'))
    // from is expected to vary
    t.isDeeply(rmFrom(actual), rmFrom(expected))
    t.done()
  }

  function rmFrom (obj) {
    for (var i in obj) {
      if (i === 'from') {
        delete obj[i]
      } else if (i === 'dependencies') {
        for (var j in obj[i]) {
          rmFrom(obj[i][j])
        }
      }
    }
  }
})

test('cleanup', function (t) {
  cleanup()
  t.done()
})

function setup () {
  cleanup()
  fixture.create(basepath)
}

function cleanup () {
  fixture.remove(basepath)
}
