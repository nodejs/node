'use strict'

const fs = require('fs')
const path = require('path')

const test = require('tap').test
const Tacks = require('tacks')
const Dir = Tacks.Dir
const File = Tacks.File
const common = require('../common-tap.js')
const isWindows = require('../../lib/utils/is-windows.js')

const base = common.pkg
const noFunding = path.join(base, 'no-funding-package')
const maintainerOwnsAllDeps = path.join(base, 'maintainer-owns-all-deps')
const nestedNoFundingPackages = path.join(base, 'nested-no-funding-packages')
const nestedMultipleFundingPackages = path.join(base, 'nested-multiple-funding-packages')
const fundingStringShorthand = path.join(base, 'funding-string-shorthand')

function getFixturePackage ({ name, version, dependencies, funding }, extras) {
  const getDeps = () => Object
    .keys(dependencies)
    .reduce((res, dep) => (Object.assign({}, res, {
      [dep]: '*'
    })), {})

  return Dir(Object.assign({
    'package.json': File({
      name,
      version: version || '1.0.0',
      funding: (funding === undefined) ? {
        type: 'individual',
        url: 'http://example.com/donate'
      } : funding,
      dependencies: dependencies && getDeps(dependencies)
    })
  }, extras))
}

const fixture = new Tacks(Dir({
  'funding-string-shorthand': Dir({
    'package.json': File({
      name: 'funding-string-shorthand',
      version: '0.0.0',
      funding: 'https://example.com/sponsor'
    })
  }),
  'no-funding-package': Dir({
    'package.json': File({
      name: 'no-funding-package',
      version: '0.0.0'
    })
  }),
  'maintainer-owns-all-deps': getFixturePackage({
    name: 'maintainer-owns-all-deps',
    dependencies: {
      'dep-foo': '*',
      'dep-bar': '*'
    }
  }, {
    node_modules: Dir({
      'dep-foo': getFixturePackage({
        name: 'dep-foo',
        dependencies: {
          'dep-sub-foo': '*'
        }
      }, {
        node_modules: Dir({
          'dep-sub-foo': getFixturePackage({
            name: 'dep-sub-foo'
          })
        })
      }),
      'dep-bar': getFixturePackage({
        name: 'dep-bar'
      })
    })
  }),
  'nested-no-funding-packages': getFixturePackage({
    name: 'nested-no-funding-packages',
    funding: null,
    dependencies: {
      foo: '*'
    },
    devDependencies: {
      lorem: '*'
    }
  }, {
    node_modules: Dir({
      foo: getFixturePackage({
        name: 'foo',
        dependencies: {
          bar: '*'
        },
        funding: null
      }, {
        node_modules: Dir({
          bar: getFixturePackage({
            name: 'bar'
          }, {
            node_modules: Dir({
              'sub-bar': getFixturePackage({
                name: 'sub-bar',
                funding: 'https://example.com/sponsor'
              })
            })
          })
        })
      }),
      lorem: getFixturePackage({
        name: 'lorem',
        funding: {
          url: 'https://example.com/lorem'
        }
      })
    })
  }),
  'nested-multiple-funding-packages': getFixturePackage({
    name: 'nested-multiple-funding-packages',
    funding: [
      'https://one.example.com',
      'https://two.example.com'
    ],
    dependencies: {
      foo: '*'
    },
    devDependencies: {
      bar: '*'
    }
  }, {
    node_modules: Dir({
      foo: getFixturePackage({
        name: 'foo',
        funding: [
          'http://example.com',
          { url: 'http://sponsors.example.com/me' },
          'http://collective.example.com'
        ]
      }),
      bar: getFixturePackage({
        name: 'bar',
        funding: [
          'http://collective.example.com',
          { url: 'http://sponsors.example.com/you' }
        ]
      })
    })
  })
}))

function checkOutput (t, { code, stdout, stderr }) {
  t.is(code, 0, `exited code 0`)
  t.is(stderr, '', 'no warnings')
}

function jsonTest (t, { assertionMsg, expected, stdout }) {
  let parsed = JSON.parse(stdout)
  t.deepEqual(parsed, expected, assertionMsg)
}

function snapshotTest (t, { stdout, assertionMsg }) {
  t.matchSnapshot(stdout, assertionMsg)
}

function testFundCmd ({ title, assertionMsg, args = [], opts = {}, output = checkOutput, assertion = snapshotTest, expected }) {
  const validate = (t) => (err, code, stdout, stderr) => {
    if (err) throw err

    output(t, { code, stdout, stderr })
    assertion(t, { assertionMsg, expected, stdout })
  }

  return test(title, (t) => {
    t.plan(3)
    common.npm(['fund', '--unicode=false'].concat(args), opts, validate(t))
  })
}

test('setup', function (t) {
  fixture.remove(base)
  fixture.create(base)
  t.end()
})

testFundCmd({
  title: 'fund with no package containing funding',
  assertionMsg: 'should print empty funding info',
  opts: { cwd: noFunding }
})

testFundCmd({
  title: 'fund in which same maintainer owns all its deps',
  assertionMsg: 'should print stack packages together',
  opts: { cwd: maintainerOwnsAllDeps }
})

testFundCmd({
  title: 'fund in which same maintainer owns all its deps, using --json option',
  assertionMsg: 'should print stack packages together',
  args: ['--json'],
  opts: { cwd: maintainerOwnsAllDeps },
  assertion: jsonTest,
  expected: {
    length: 3,
    name: 'maintainer-owns-all-deps',
    version: '1.0.0',
    funding: { type: 'individual', url: 'http://example.com/donate' },
    dependencies: {
      'dep-bar': {
        version: '1.0.0',
        funding: { type: 'individual', url: 'http://example.com/donate' }
      },
      'dep-foo': {
        version: '1.0.0',
        funding: { type: 'individual', url: 'http://example.com/donate' },
        dependencies: {
          'dep-sub-foo': {
            version: '1.0.0',
            funding: { type: 'individual', url: 'http://example.com/donate' }
          }
        }
      }
    }
  }
})

testFundCmd({
  title: 'fund containing multi-level nested deps with no funding',
  assertionMsg: 'should omit dependencies with no funding declared',
  opts: { cwd: nestedNoFundingPackages }
})

testFundCmd({
  title: 'fund containing multi-level nested deps with no funding, using --json option',
  assertionMsg: 'should omit dependencies with no funding declared',
  args: ['--json'],
  opts: { cwd: nestedNoFundingPackages },
  assertion: jsonTest,
  expected: {
    length: 3,
    name: 'nested-no-funding-packages',
    version: '1.0.0',
    dependencies: {
      lorem: { version: '1.0.0', funding: { url: 'https://example.com/lorem' } },
      bar: {
        version: '1.0.0',
        funding: { type: 'individual', url: 'http://example.com/donate' },
        dependencies: {
          'sub-bar': {
            version: '1.0.0',
            funding: { url: 'https://example.com/sponsor' }
          }
        }
      }
    }
  }
})

testFundCmd({
  title: 'fund containing multi-level nested deps with multiple funding sources, using --json option',
  assertionMsg: 'should omit dependencies with no funding declared',
  args: ['--json'],
  opts: { cwd: nestedMultipleFundingPackages },
  assertion: jsonTest,
  expected: {
    length: 2,
    name: 'nested-multiple-funding-packages',
    version: '1.0.0',
    funding: [
      {
        url: 'https://one.example.com'
      },
      {
        url: 'https://two.example.com'
      }
    ],
    dependencies: {
      bar: {
        version: '1.0.0',
        funding: [
          {
            url: 'http://collective.example.com'
          },
          {
            url: 'http://sponsors.example.com/you'
          }
        ]
      },
      foo: {
        version: '1.0.0',
        funding: [
          {
            url: 'http://example.com'
          },
          {
            url: 'http://sponsors.example.com/me'
          },
          {
            url: 'http://collective.example.com'
          }
        ]
      }
    }
  }
})

testFundCmd({
  title: 'fund does not support global',
  assertionMsg: 'should throw EFUNDGLOBAL error',
  args: ['--global'],
  output: (t, { code, stdout, stderr }) => {
    t.is(code, 1, `exited code 0`)
    const [ errCode, errCmd ] = stderr.split('\n')
    t.matchSnapshot(`${errCode}\n${errCmd}`, 'should write error msgs to stderr')
  }
})

testFundCmd({
  title: 'fund does not support global, using --json option',
  assertionMsg: 'should throw EFUNDGLOBAL error',
  args: ['--global', '--json'],
  output: (t, { code, stdout, stderr }) => {
    t.is(code, 1, `exited code 0`)
    const [ errCode, errCmd ] = stderr.split('\n')
    t.matchSnapshot(`${errCode}\n${errCmd}`, 'should write error msgs to stderr')
  },
  assertion: jsonTest,
  expected: {
    error: {
      code: 'EFUNDGLOBAL',
      summary: '`npm fund` does not support global packages',
      detail: ''
    }
  }
})

testFundCmd({
  title: 'fund using package argument with no browser',
  assertionMsg: 'should open funding url',
  args: ['.', '--no-browser'],
  opts: { cwd: maintainerOwnsAllDeps }
})

testFundCmd({
  title: 'fund using string shorthand',
  assertionMsg: 'should open string-only url',
  args: ['.', '--no-browser'],
  opts: { cwd: fundingStringShorthand }
})

testFundCmd({
  title: 'fund using nested packages with multiple sources',
  assertionMsg: 'should prompt with all available URLs',
  args: ['.'],
  opts: { cwd: nestedMultipleFundingPackages }
})

testFundCmd({
  title: 'fund using nested packages with multiple sources, with a source number',
  assertionMsg: 'should open the numbered URL',
  args: ['.', '--which=1', '--no-browser'],
  opts: { cwd: nestedMultipleFundingPackages }
})

testFundCmd({
  title: 'fund using package argument with no browser, using --json option',
  assertionMsg: 'should open funding url',
  args: ['.', '--json', '--no-browser'],
  opts: { cwd: maintainerOwnsAllDeps },
  assertion: jsonTest,
  expected: {
    title: 'individual funding available at the following URL',
    url: 'http://example.com/donate'
  }
})

if (!isWindows) {
  test('fund using package argument', function (t) {
    const fakeBrowser = path.join(common.pkg, '_script.sh')
    const outFile = path.join(common.pkg, '_output')

    const s = '#!/usr/bin/env bash\n' +
            'echo "$@" > ' + JSON.stringify(common.pkg) + '/_output\n'
    fs.writeFileSync(fakeBrowser, s)
    fs.chmodSync(fakeBrowser, '0755')

    common.npm([
      'fund', '.',
      '--loglevel=silent',
      '--browser=' + fakeBrowser
    ], { cwd: maintainerOwnsAllDeps }, function (err, code, stdout, stderr) {
      t.ifError(err, 'repo command ran without error')
      t.equal(code, 0, 'exit ok')
      var res = fs.readFileSync(outFile, 'utf8')
      t.equal(res, 'http://example.com/donate\n')
      t.end()
    })
  })
}

test('cleanup', function (t) {
  t.pass(base)
  fixture.remove(base)
  t.end()
})
