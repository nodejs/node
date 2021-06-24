const t = require('tap')
const { fake: mockNpm } = require('../fixtures/mock-npm')

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
const config = {
  color: false,
  json: false,
  global: false,
  unicode: false,
  which: null,
}
const openUrl = async (npm, url, msg) => {
  if (url === 'http://npmjs.org')
    throw new Error('ERROR')

  if (config.json) {
    printUrl = JSON.stringify({
      title: msg,
      url: url,
    })
  } else
    printUrl = `${msg}:\n  ${url}`
}
const Fund = t.mock('../../lib/fund.js', {
  '../../lib/utils/open-url.js': openUrl,
  pacote: {
    manifest: (arg) => arg.name === 'ntl'
      ? Promise.resolve({
        funding: 'http://example.com/pacote',
      })
      : Promise.reject(new Error('ERROR')),
  },
})
const npm = mockNpm({
  config,
  output: msg => {
    result += msg + '\n'
  },
})
const fund = new Fund(npm)

t.test('fund with no package containing funding', t => {
  npm.prefix = t.testdir({
    'package.json': JSON.stringify({
      name: 'no-funding-package',
      version: '0.0.0',
    }),
  })

  fund.exec([], (err) => {
    t.error(err, 'should not error out')
    t.matchSnapshot(result, 'should print empty funding info')
    result = ''
    t.end()
  })
})

t.test('fund in which same maintainer owns all its deps', t => {
  npm.prefix = t.testdir(maintainerOwnsAllDeps)

  fund.exec([], (err) => {
    t.error(err, 'should not error out')
    t.matchSnapshot(result, 'should print stack packages together')
    result = ''
    t.end()
  })
})

t.test('fund in which same maintainer owns all its deps, using --json option', t => {
  config.json = true
  npm.prefix = t.testdir(maintainerOwnsAllDeps)

  fund.exec([], (err) => {
    t.error(err, 'should not error out')
    t.same(
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
    config.json = false
    t.end()
  })
})

t.test('fund containing multi-level nested deps with no funding', t => {
  npm.prefix = t.testdir(nestedNoFundingPackages)

  fund.exec([], (err) => {
    t.error(err, 'should not error out')
    t.matchSnapshot(
      result,
      'should omit dependencies with no funding declared'
    )

    result = ''
    t.end()
  })
})

t.test('fund containing multi-level nested deps with no funding, using --json option', t => {
  npm.prefix = t.testdir(nestedNoFundingPackages)
  config.json = true

  fund.exec([], (err) => {
    t.error(err, 'should not error out')
    t.same(
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
    config.json = false
    t.end()
  })
})

t.test('fund containing multi-level nested deps with no funding, using --json option', t => {
  npm.prefix = t.testdir(nestedMultipleFundingPackages)
  config.json = true

  fund.exec([], (err) => {
    t.error(err, 'should not error out')
    t.same(
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
    config.json = false
    t.end()
  })
})

t.test('fund does not support global', t => {
  npm.prefix = t.testdir({})
  config.global = true

  fund.exec([], (err) => {
    t.match(err.code, 'EFUNDGLOBAL', 'should throw EFUNDGLOBAL error')

    result = ''
    config.global = false
    t.end()
  })
})

t.test('fund using package argument', t => {
  npm.prefix = t.testdir(maintainerOwnsAllDeps)

  fund.exec(['.'], (err) => {
    t.error(err, 'should not error out')
    t.matchSnapshot(printUrl, 'should open funding url')

    printUrl = ''
    t.end()
  })
})

t.test('fund does not support global, using --json option', t => {
  npm.prefix = t.testdir({})
  config.global = true
  config.json = true

  fund.exec([], (err) => {
    t.equal(err.code, 'EFUNDGLOBAL', 'should use EFUNDGLOBAL error code')
    t.equal(
      err.message,
      '`npm fund` does not support global packages',
      'should use expected error msg'
    )

    config.global = false
    config.json = false
    t.end()
  })
})

t.test('fund using string shorthand', t => {
  npm.prefix = t.testdir({
    'package.json': JSON.stringify({
      name: 'funding-string-shorthand',
      version: '0.0.0',
      funding: 'https://example.com/sponsor',
    }),
  })

  fund.exec(['.'], (err) => {
    t.error(err, 'should not error out')
    t.matchSnapshot(printUrl, 'should open string-only url')

    printUrl = ''
    t.end()
  })
})

t.test('fund using nested packages with multiple sources', t => {
  npm.prefix = t.testdir(nestedMultipleFundingPackages)

  fund.exec(['.'], (err) => {
    t.error(err, 'should not error out')
    t.matchSnapshot(result, 'should prompt with all available URLs')

    result = ''
    t.end()
  })
})

t.test('fund using symlink ref', t => {
  npm.prefix = t.testdir({
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
    t.error(err, 'should not error out')
    t.match(
      printUrl,
      'http://example.com/a',
      'should retrieve funding url from symlink'
    )

    printUrl = ''

    // using target ref
    fund.exec(['./a'], (err) => {
      t.error(err, 'should not error out')

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

t.test('fund using data from actual tree', t => {
  npm.prefix = t.testdir({
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
    t.error(err, 'should not error out')
    t.match(
      printUrl,
      'http://example.com/_AAA',
      'should retrieve fund info from actual tree, using greatest version found'
    )

    printUrl = ''
    t.end()
  })
})

t.test('fund using nested packages with multiple sources, with a source number', t => {
  npm.prefix = t.testdir(nestedMultipleFundingPackages)
  config.which = '1'

  fund.exec(['.'], (err) => {
    t.error(err, 'should not error out')
    t.matchSnapshot(printUrl, 'should open the numbered URL')

    config.which = null
    printUrl = ''
    t.end()
  })
})

t.test('fund using pkg name while having conflicting versions', t => {
  npm.prefix = t.testdir(conflictingFundingPackages)
  config.which = '1'

  fund.exec(['foo'], (err) => {
    t.error(err, 'should not error out')
    t.matchSnapshot(printUrl, 'should open greatest version')

    printUrl = ''
    t.end()
  })
})

t.test('fund using package argument with no browser, using --json option', t => {
  npm.prefix = t.testdir(maintainerOwnsAllDeps)
  config.json = true

  fund.exec(['.'], (err) => {
    t.error(err, 'should not error out')
    t.same(
      JSON.parse(printUrl),
      {
        title: 'individual funding available at the following URL',
        url: 'http://example.com/donate',
      },
      'should open funding url using json output'
    )

    config.json = false
    printUrl = ''
    t.end()
  })
})

t.test('fund using package info fetch from registry', t => {
  npm.prefix = t.testdir({})

  fund.exec(['ntl'], (err) => {
    t.error(err, 'should not error out')
    t.match(
      printUrl,
      /http:\/\/example.com\/pacote/,
      'should open funding url that was loaded from registry manifest'
    )

    printUrl = ''
    t.end()
  })
})

t.test('fund tries to use package info fetch from registry but registry has nothing', t => {
  npm.prefix = t.testdir({})

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

t.test('fund but target module has no funding info', t => {
  npm.prefix = t.testdir(nestedNoFundingPackages)

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

t.test('fund using bad which value', t => {
  npm.prefix = t.testdir(nestedMultipleFundingPackages)
  config.which = 3

  fund.exec(['bar'], (err) => {
    t.equal(err.code, 'EFUNDNUMBER', 'should have EFUNDNUMBER error code')
    t.equal(
      err.message,
      '`npm fund [<@scope>/]<pkg> [--which=fundingSourceNumber]` must be given a positive integer',
      'should have bad which option error message'
    )

    config.which = null
    result = ''
    t.end()
  })
})

t.test('fund pkg missing version number', t => {
  npm.prefix = t.testdir({
    'package.json': JSON.stringify({
      name: 'foo',
      funding: 'http://example.com/foo',
    }),
  })

  fund.exec([], (err) => {
    t.error(err, 'should not error out')
    t.matchSnapshot(result, 'should print name only')
    result = ''
    t.end()
  })
})

t.test('fund a package throws on openUrl', t => {
  npm.prefix = t.testdir({
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

t.test('fund a package with type and multiple sources', t => {
  npm.prefix = t.testdir({
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
    t.error(err, 'should not error out')
    t.matchSnapshot(result, 'should print prompt select message')

    result = ''
    t.end()
  })
})

t.test('fund colors', t => {
  npm.prefix = t.testdir({
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
  npm.color = true

  fund.exec([], (err) => {
    t.error(err, 'should not error out')
    t.matchSnapshot(result, 'should print output with color info')

    result = ''
    npm.color = false
    t.end()
  })
})

t.test('sub dep with fund info and a parent with no funding info', t => {
  npm.prefix = t.testdir({
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
    t.error(err, 'should not error out')
    t.matchSnapshot(result, 'should nest sub dep as child of root')

    result = ''
    t.end()
  })
})

t.test('workspaces', t => {
  t.test('filter funding info by a specific workspace', async t => {
    npm.localPrefix = npm.prefix = t.testdir({
      'package.json': JSON.stringify({
        name: 'workspaces-support',
        version: '1.0.0',
        workspaces: ['packages/*'],
        dependencies: {
          d: '^1.0.0',
        },
      }),
      node_modules: {
        a: t.fixture('symlink', '../packages/a'),
        b: t.fixture('symlink', '../packages/b'),
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
        d: {
          'package.json': JSON.stringify({
            name: 'd',
            version: '1.0.0',
            funding: 'http://example.com/d',
          }),
        },
      },
      packages: {
        a: {
          'package.json': JSON.stringify({
            name: 'a',
            version: '1.0.0',
            funding: 'https://example.com/a',
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
            dependencies: {
              d: '^1.0.0',
            },
          }),
        },
      },
    })

    await new Promise((res, rej) => {
      fund.execWorkspaces([], ['a'], (err) => {
        if (err)
          rej(err)

        t.matchSnapshot(result,
          'should display only filtered workspace name and its deps')

        result = ''
        res()
      })
    })

    await new Promise((res, rej) => {
      fund.execWorkspaces([], ['./packages/a'], (err) => {
        if (err)
          rej(err)

        t.matchSnapshot(result,
          'should display only filtered workspace path and its deps')

        result = ''
        res()
      })
    })
  })

  t.end()
})
