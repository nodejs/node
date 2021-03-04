const { test } = require('tap')
const requireInject = require('require-inject')

const version = '1.0.0'
const funding = {
  type: 'individual',
  url: 'http://example.com/donate',
}

const maintainerOwnsAllDeps = {
  'package.json': JSON.stringify({
    name: 'maintainer-owns-all-deps',
    version,
    funding,
    dependencies: {
      'dep-foo': '*',
      'dep-bar': '*',
    },
  }),
  node_modules: {
    'dep-foo': {
      'package.json': JSON.stringify({
        name: 'dep-foo',
        version,
        funding,
        dependencies: {
          'dep-sub-foo': '*',
        },
      }),
      node_modules: {
        'dep-sub-foo': {
          'package.json': JSON.stringify({
            name: 'dep-sub-foo',
            version,
            funding,
          }),
        },
      },
    },
    'dep-bar': {
      'package.json': JSON.stringify({
        name: 'dep-bar',
        version,
        funding,
      }),
    },
  },
}

const nestedNoFundingPackages = {
  'package.json': JSON.stringify({
    name: 'nested-no-funding-packages',
    version,
    dependencies: {
      foo: '*',
    },
    devDependencies: {
      lorem: '*',
    },
  }),
  node_modules: {
    foo: {
      'package.json': JSON.stringify({
        name: 'foo',
        version,
        dependencies: {
          bar: '*',
        },
      }),
      node_modules: {
        bar: {
          'package.json': JSON.stringify({
            name: 'bar',
            version,
            funding,
          }),
          node_modules: {
            'sub-bar': {
              'package.json': JSON.stringify({
                name: 'sub-bar',
                version,
                funding: 'https://example.com/sponsor',
              }),
            },
          },
        },
      },
    },
    lorem: {
      'package.json': JSON.stringify({
        name: 'lorem',
        version,
        funding: {
          url: 'https://example.com/lorem',
        },
      }),
    },
  },
}

const nestedMultipleFundingPackages = {
  'package.json': JSON.stringify({
    name: 'nested-multiple-funding-packages',
    version,
    funding: [
      'https://one.example.com',
      'https://two.example.com',
    ],
    dependencies: {
      foo: '*',
    },
    devDependencies: {
      bar: '*',
    },
  }),
  node_modules: {
    foo: {
      'package.json': JSON.stringify({
        name: 'foo',
        version,
        funding: [
          'http://example.com',
          { url: 'http://sponsors.example.com/me' },
          'http://collective.example.com',
        ],
      }),
    },
    bar: {
      'package.json': JSON.stringify({
        name: 'bar',
        version,
        funding: [
          'http://collective.example.com',
          { url: 'http://sponsors.example.com/you' },
        ],
      }),
    },
  },
}

const conflictingFundingPackages = {
  'package.json': JSON.stringify({
    name: 'conflicting-funding-packages',
    version,
    dependencies: {
      foo: '1.0.0',
    },
    devDependencies: {
      bar: '1.0.0',
    },
  }),
  node_modules: {
    foo: {
      'package.json': JSON.stringify({
        name: 'foo',
        version: '1.0.0',
        funding: 'http://example.com/1',
      }),
    },
    bar: {
      node_modules: {
        foo: {
          'package.json': JSON.stringify({
            name: 'foo',
            version: '2.0.0',
            funding: 'http://example.com/2',
          }),
        },
      },
      'package.json': JSON.stringify({
        name: 'bar',
        version: '1.0.0',
        dependencies: {
          foo: '2.0.0',
        },
      }),
    },
  },
}

let result = ''
let printUrl = ''
const _flatOptions = {
  color: false,
  json: false,
  global: false,
  prefix: undefined,
  unicode: false,
  which: undefined,
}
const openUrl = async (npm, url, msg) => {
  if (url === 'http://npmjs.org')
    throw new Error('ERROR')

  if (_flatOptions.json) {
    printUrl = JSON.stringify({
      title: msg,
      url: url,
    })
  } else
    printUrl = `${msg}:\n  ${url}`
}
const Fund = requireInject('../../lib/fund.js', {
  '../../lib/utils/open-url.js': openUrl,
  '../../lib/utils/output.js': msg => {
    result += msg + '\n'
  },
  pacote: {
    manifest: (arg) => arg.name === 'ntl'
      ? Promise.resolve({
        funding: 'http://example.com/pacote',
      })
      : Promise.reject(new Error('ERROR')),
  },
})
const fund = new Fund({
  flatOptions: _flatOptions,
  get prefix () {
    return _flatOptions.prefix
  },
})

test('fund with no package containing funding', t => {
  _flatOptions.prefix = t.testdir({
    'package.json': JSON.stringify({
      name: 'no-funding-package',
      version: '0.0.0',
    }),
  })

  fund.exec([], (err) => {
    t.ifError(err, 'should not error out')
    t.matchSnapshot(result, 'should print empty funding info')
    result = ''
    t.end()
  })
})

test('fund in which same maintainer owns all its deps', t => {
  _flatOptions.prefix = t.testdir(maintainerOwnsAllDeps)

  fund.exec([], (err) => {
    t.ifError(err, 'should not error out')
    t.matchSnapshot(result, 'should print stack packages together')
    result = ''
    t.end()
  })
})

test('fund in which same maintainer owns all its deps, using --json option', t => {
  _flatOptions.json = true
  _flatOptions.prefix = t.testdir(maintainerOwnsAllDeps)

  fund.exec([], (err) => {
    t.ifError(err, 'should not error out')
    t.deepEqual(
      JSON.parse(result),
      {
        length: 3,
        name: 'maintainer-owns-all-deps',
        version: '1.0.0',
        funding: { type: 'individual', url: 'http://example.com/donate' },
        dependencies: {
          'dep-bar': {
            version: '1.0.0',
            funding: { type: 'individual', url: 'http://example.com/donate' },
          },
          'dep-foo': {
            version: '1.0.0',
            funding: { type: 'individual', url: 'http://example.com/donate' },
            dependencies: {
              'dep-sub-foo': {
                version: '1.0.0',
                funding: { type: 'individual', url: 'http://example.com/donate' },
              },
            },
          },
        },
      },
      'should print stack packages together'
    )

    result = ''
    _flatOptions.json = false
    t.end()
  })
})

test('fund containing multi-level nested deps with no funding', t => {
  _flatOptions.prefix = t.testdir(nestedNoFundingPackages)

  fund.exec([], (err) => {
    t.ifError(err, 'should not error out')
    t.matchSnapshot(
      result,
      'should omit dependencies with no funding declared'
    )

    result = ''
    t.end()
  })
})

test('fund containing multi-level nested deps with no funding, using --json option', t => {
  _flatOptions.prefix = t.testdir(nestedNoFundingPackages)
  _flatOptions.json = true

  fund.exec([], (err) => {
    t.ifError(err, 'should not error out')
    t.deepEqual(
      JSON.parse(result),
      {
        length: 2,
        name: 'nested-no-funding-packages',
        version: '1.0.0',
        dependencies: {
          lorem: {
            version: '1.0.0',
            funding: { url: 'https://example.com/lorem' },
          },
          bar: {
            version: '1.0.0',
            funding: { type: 'individual', url: 'http://example.com/donate' },
          },
        },
      },
      'should omit dependencies with no funding declared in json output'
    )

    result = ''
    _flatOptions.json = false
    t.end()
  })
})

test('fund containing multi-level nested deps with no funding, using --json option', t => {
  _flatOptions.prefix = t.testdir(nestedMultipleFundingPackages)
  _flatOptions.json = true

  fund.exec([], (err) => {
    t.ifError(err, 'should not error out')
    t.deepEqual(
      JSON.parse(result),
      {
        length: 2,
        name: 'nested-multiple-funding-packages',
        version: '1.0.0',
        funding: [
          {
            url: 'https://one.example.com',
          },
          {
            url: 'https://two.example.com',
          },
        ],
        dependencies: {
          bar: {
            version: '1.0.0',
            funding: [
              {
                url: 'http://collective.example.com',
              },
              {
                url: 'http://sponsors.example.com/you',
              },
            ],
          },
          foo: {
            version: '1.0.0',
            funding: [
              {
                url: 'http://example.com',
              },
              {
                url: 'http://sponsors.example.com/me',
              },
              {
                url: 'http://collective.example.com',
              },
            ],
          },
        },
      },
      'should list multiple funding entries in json output'
    )

    result = ''
    _flatOptions.json = false
    t.end()
  })
})

test('fund does not support global', t => {
  _flatOptions.prefix = t.testdir({})
  _flatOptions.global = true

  fund.exec([], (err) => {
    t.match(err.code, 'EFUNDGLOBAL', 'should throw EFUNDGLOBAL error')

    result = ''
    _flatOptions.global = false
    t.end()
  })
})

test('fund using package argument', t => {
  _flatOptions.prefix = t.testdir(maintainerOwnsAllDeps)

  fund.exec(['.'], (err) => {
    t.ifError(err, 'should not error out')
    t.matchSnapshot(printUrl, 'should open funding url')

    printUrl = ''
    t.end()
  })
})

test('fund does not support global, using --json option', t => {
  _flatOptions.prefix = t.testdir({})
  _flatOptions.global = true
  _flatOptions.json = true

  fund.exec([], (err) => {
    t.equal(err.code, 'EFUNDGLOBAL', 'should use EFUNDGLOBAL error code')
    t.equal(
      err.message,
      '`npm fund` does not support global packages',
      'should use expected error msg'
    )

    _flatOptions.global = false
    _flatOptions.json = false
    t.end()
  })
})

test('fund using string shorthand', t => {
  _flatOptions.prefix = t.testdir({
    'package.json': JSON.stringify({
      name: 'funding-string-shorthand',
      version: '0.0.0',
      funding: 'https://example.com/sponsor',
    }),
  })

  fund.exec(['.'], (err) => {
    t.ifError(err, 'should not error out')
    t.matchSnapshot(printUrl, 'should open string-only url')

    printUrl = ''
    t.end()
  })
})

test('fund using nested packages with multiple sources', t => {
  _flatOptions.prefix = t.testdir(nestedMultipleFundingPackages)

  fund.exec(['.'], (err) => {
    t.ifError(err, 'should not error out')
    t.matchSnapshot(result, 'should prompt with all available URLs')

    result = ''
    t.end()
  })
})

test('fund using symlink ref', t => {
  _flatOptions.prefix = t.testdir({
    'package.json': JSON.stringify({
      name: 'using-symlink-ref',
      version: '1.0.0',
    }),
    a: {
      'package.json': JSON.stringify({
        name: 'a',
        version: '1.0.0',
        funding: 'http://example.com/a',
      }),
    },
    node_modules: {
      a: t.fixture('symlink', '../a'),
    },
  })

  // using symlinked ref
  fund.exec(['./node_modules/a'], (err) => {
    t.ifError(err, 'should not error out')
    t.match(
      printUrl,
      'http://example.com/a',
      'should retrieve funding url from symlink'
    )

    printUrl = ''

    // using target ref
    fund.exec(['./a'], (err) => {
      t.ifError(err, 'should not error out')

      t.match(
        printUrl,
        'http://example.com/a',
        'should retrieve funding url from symlink target'
      )

      printUrl = ''
      result = ''
      t.end()
    })
  })
})

test('fund using data from actual tree', t => {
  _flatOptions.prefix = t.testdir({
    'package.json': JSON.stringify({
      name: 'using-actual-tree',
      version: '1.0.0',
    }),
    node_modules: {
      a: {
        'package.json': JSON.stringify({
          name: 'a',
          version: '1.0.0',
          funding: 'http://example.com/a',
        }),
      },
      b: {
        'package.json': JSON.stringify({
          name: 'a',
          version: '1.0.0',
          funding: 'http://example.com/b',
        }),
        node_modules: {
          a: {
            'package.json': JSON.stringify({
              name: 'a',
              version: '1.0.1',
              funding: 'http://example.com/_AAA',
            }),
          },
        },
      },
    },
  })

  // using symlinked ref
  fund.exec(['a'], (err) => {
    t.ifError(err, 'should not error out')
    t.match(
      printUrl,
      'http://example.com/_AAA',
      'should retrieve fund info from actual tree, using greatest version found'
    )

    printUrl = ''
    t.end()
  })
})

test('fund using nested packages with multiple sources, with a source number', t => {
  _flatOptions.prefix = t.testdir(nestedMultipleFundingPackages)
  _flatOptions.which = '1'

  fund.exec(['.'], (err) => {
    t.ifError(err, 'should not error out')
    t.matchSnapshot(printUrl, 'should open the numbered URL')

    _flatOptions.which = undefined
    printUrl = ''
    t.end()
  })
})

test('fund using pkg name while having conflicting versions', t => {
  _flatOptions.prefix = t.testdir(conflictingFundingPackages)
  _flatOptions.which = '1'

  fund.exec(['foo'], (err) => {
    t.ifError(err, 'should not error out')
    t.matchSnapshot(printUrl, 'should open greatest version')

    printUrl = ''
    t.end()
  })
})

test('fund using package argument with no browser, using --json option', t => {
  _flatOptions.prefix = t.testdir(maintainerOwnsAllDeps)
  _flatOptions.json = true

  fund.exec(['.'], (err) => {
    t.ifError(err, 'should not error out')
    t.deepEqual(
      JSON.parse(printUrl),
      {
        title: 'individual funding available at the following URL',
        url: 'http://example.com/donate',
      },
      'should open funding url using json output'
    )

    _flatOptions.json = false
    printUrl = ''
    t.end()
  })
})

test('fund using package info fetch from registry', t => {
  _flatOptions.prefix = t.testdir({})

  fund.exec(['ntl'], (err) => {
    t.ifError(err, 'should not error out')
    t.match(
      printUrl,
      /http:\/\/example.com\/pacote/,
      'should open funding url that was loaded from registry manifest'
    )

    printUrl = ''
    t.end()
  })
})

test('fund tries to use package info fetch from registry but registry has nothing', t => {
  _flatOptions.prefix = t.testdir({})

  fund.exec(['foo'], (err) => {
    t.equal(err.code, 'ENOFUND', 'should have ENOFUND error code')
    t.equal(
      err.message,
      'No valid funding method available for: foo',
      'should have no valid funding message'
    )

    printUrl = ''
    t.end()
  })
})

test('fund but target module has no funding info', t => {
  _flatOptions.prefix = t.testdir(nestedNoFundingPackages)

  fund.exec(['foo'], (err) => {
    t.equal(err.code, 'ENOFUND', 'should have ENOFUND error code')
    t.equal(
      err.message,
      'No valid funding method available for: foo',
      'should have no valid funding message'
    )

    result = ''
    t.end()
  })
})

test('fund using bad which value', t => {
  _flatOptions.prefix = t.testdir(nestedMultipleFundingPackages)
  _flatOptions.which = 3

  fund.exec(['bar'], (err) => {
    t.equal(err.code, 'EFUNDNUMBER', 'should have EFUNDNUMBER error code')
    t.equal(
      err.message,
      '`npm fund [<@scope>/]<pkg> [--which=fundingSourceNumber]` must be given a positive integer',
      'should have bad which option error message'
    )

    _flatOptions.which = undefined
    result = ''
    t.end()
  })
})

test('fund pkg missing version number', t => {
  _flatOptions.prefix = t.testdir({
    'package.json': JSON.stringify({
      name: 'foo',
      funding: 'http://example.com/foo',
    }),
  })

  fund.exec([], (err) => {
    t.ifError(err, 'should not error out')
    t.matchSnapshot(result, 'should print name only')
    result = ''
    t.end()
  })
})

test('fund a package throws on openUrl', t => {
  _flatOptions.prefix = t.testdir({
    'package.json': JSON.stringify({
      name: 'foo',
      version: '1.0.0',
      funding: 'http://npmjs.org',
    }),
  })

  fund.exec(['.'], (err) => {
    t.equal(err.message, 'ERROR', 'should throw unknown error')
    result = ''
    t.end()
  })
})

test('fund a package with type and multiple sources', t => {
  _flatOptions.prefix = t.testdir({
    'package.json': JSON.stringify({
      name: 'foo',
      funding: [
        {
          type: 'Foo',
          url: 'http://example.com/foo',
        },
        {
          type: 'Lorem',
          url: 'http://example.com/foo-lorem',
        },
      ],
    }),
  })

  fund.exec(['.'], (err) => {
    t.ifError(err, 'should not error out')
    t.matchSnapshot(result, 'should print prompt select message')

    result = ''
    t.end()
  })
})

test('fund colors', t => {
  _flatOptions.prefix = t.testdir({
    'package.json': JSON.stringify({
      name: 'test-fund-colors',
      version: '1.0.0',
      dependencies: {
        a: '^1.0.0',
        b: '^1.0.0',
        c: '^1.0.0',
      },
    }),
    node_modules: {
      a: {
        'package.json': JSON.stringify({
          name: 'a',
          version: '1.0.0',
          funding: 'http://example.com/a',
        }),
      },
      b: {
        'package.json': JSON.stringify({
          name: 'b',
          version: '1.0.0',
          funding: 'http://example.com/b',
          dependencies: {
            d: '^1.0.0',
            e: '^1.0.0',
          },
        }),
      },
      c: {
        'package.json': JSON.stringify({
          name: 'c',
          version: '1.0.0',
          funding: 'http://example.com/b',
        }),
      },
      d: {
        'package.json': JSON.stringify({
          name: 'd',
          version: '1.0.0',
          funding: 'http://example.com/d',
        }),
      },
      e: {
        'package.json': JSON.stringify({
          name: 'e',
          version: '1.0.0',
          funding: 'http://example.com/e',
        }),
      },
    },
  })
  _flatOptions.color = true

  fund.exec([], (err) => {
    t.ifError(err, 'should not error out')
    t.matchSnapshot(result, 'should print output with color info')

    result = ''
    _flatOptions.color = false
    t.end()
  })
})

test('sub dep with fund info and a parent with no funding info', t => {
  _flatOptions.prefix = t.testdir({
    'package.json': JSON.stringify({
      name: 'test-multiple-funding-sources',
      version: '1.0.0',
      dependencies: {
        a: '^1.0.0',
        b: '^1.0.0',
      },
    }),
    node_modules: {
      a: {
        'package.json': JSON.stringify({
          name: 'a',
          version: '1.0.0',
          dependencies: {
            c: '^1.0.0',
          },
        }),
      },
      b: {
        'package.json': JSON.stringify({
          name: 'b',
          version: '1.0.0',
          funding: 'http://example.com/b',
        }),
      },
      c: {
        'package.json': JSON.stringify({
          name: 'c',
          version: '1.0.0',
          funding: [
            'http://example.com/c',
            'http://example.com/c-other',
          ],
        }),
      },
    },
  })

  fund.exec([], (err) => {
    t.ifError(err, 'should not error out')
    t.matchSnapshot(result, 'should nest sub dep as child of root')

    result = ''
    t.end()
  })
})
