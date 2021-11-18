const t = require('tap')
const { fake: mockNpm } = require('../../fixtures/mock-npm')

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
    funding: ['https://one.example.com', 'https://two.example.com'],
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
        funding: ['http://collective.example.com', { url: 'http://sponsors.example.com/you' }],
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
  if (url === 'http://npmjs.org') {
    throw new Error('ERROR')
  }

  if (config.json) {
    printUrl = JSON.stringify({
      title: msg,
      url: url,
    })
  } else {
    printUrl = `${msg}:\n  ${url}`
  }
}
const Fund = t.mock('../../../lib/commands/fund.js', {
  '../../../lib/utils/open-url.js': openUrl,
  pacote: {
    manifest: arg =>
      arg.name === 'ntl'
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

t.afterEach(() => {
  printUrl = ''
  result = ''
})
t.test('fund with no package containing funding', async t => {
  npm.prefix = t.testdir({
    'package.json': JSON.stringify({
      name: 'no-funding-package',
      version: '0.0.0',
    }),
  })

  await fund.exec([])
  t.matchSnapshot(result, 'should print empty funding info')
})

t.test('fund in which same maintainer owns all its deps', async t => {
  npm.prefix = t.testdir(maintainerOwnsAllDeps)

  await fund.exec([])
  t.matchSnapshot(result, 'should print stack packages together')
})

t.test('fund in which same maintainer owns all its deps, using --json option', async t => {
  config.json = true
  npm.prefix = t.testdir(maintainerOwnsAllDeps)

  await fund.exec([])
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
  config.json = false
})

t.test('fund containing multi-level nested deps with no funding', async t => {
  npm.prefix = t.testdir(nestedNoFundingPackages)

  await fund.exec([])
  t.matchSnapshot(result, 'should omit dependencies with no funding declared')
  t.end()
})

t.test('fund containing multi-level nested deps with no funding, using --json option', async t => {
  npm.prefix = t.testdir(nestedNoFundingPackages)
  config.json = true

  await fund.exec([])
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
  config.json = false
})

t.test('fund containing multi-level nested deps with no funding, using --json option', async t => {
  npm.prefix = t.testdir(nestedMultipleFundingPackages)
  config.json = true

  await fund.exec([])
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
  config.json = false
})

t.test('fund does not support global', async t => {
  npm.prefix = t.testdir({})
  config.global = true

  await t.rejects(fund.exec([]), { code: 'EFUNDGLOBAL' }, 'should throw EFUNDGLOBAL error')
  config.global = false
})

t.test('fund using package argument', async t => {
  npm.prefix = t.testdir(maintainerOwnsAllDeps)

  await fund.exec(['.'])
  t.matchSnapshot(printUrl, 'should open funding url')
})

t.test('fund does not support global, using --json option', async t => {
  npm.prefix = t.testdir({})
  config.global = true
  config.json = true

  await t.rejects(
    fund.exec([]),
    { code: 'EFUNDGLOBAL', message: '`npm fund` does not support global packages' },
    'should use expected error msg'
  )
  config.global = false
  config.json = false
})

t.test('fund using string shorthand', async t => {
  npm.prefix = t.testdir({
    'package.json': JSON.stringify({
      name: 'funding-string-shorthand',
      version: '0.0.0',
      funding: 'https://example.com/sponsor',
    }),
  })

  await fund.exec(['.'])
  t.matchSnapshot(printUrl, 'should open string-only url')
})

t.test('fund using nested packages with multiple sources', async t => {
  npm.prefix = t.testdir(nestedMultipleFundingPackages)

  await fund.exec(['.'])
  t.matchSnapshot(result, 'should prompt with all available URLs')
})

t.test('fund using symlink ref', async t => {
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
  await fund.exec(['./node_modules/a'])
  t.match(printUrl, 'http://example.com/a', 'should retrieve funding url from symlink')

  printUrl = ''
  result = ''

  // using target ref
  await fund.exec(['./a'])

  t.match(printUrl, 'http://example.com/a', 'should retrieve funding url from symlink target')
})

t.test('fund using data from actual tree', async t => {
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
  await fund.exec(['a'])
  t.match(
    printUrl,
    'http://example.com/_AAA',
    'should retrieve fund info from actual tree, using greatest version found'
  )
})

t.test('fund using nested packages with multiple sources, with a source number', async t => {
  npm.prefix = t.testdir(nestedMultipleFundingPackages)
  config.which = '1'

  await fund.exec(['.'])
  t.matchSnapshot(printUrl, 'should open the numbered URL')
  config.which = null
})

t.test('fund using pkg name while having conflicting versions', async t => {
  npm.prefix = t.testdir(conflictingFundingPackages)
  config.which = '1'

  await fund.exec(['foo'])
  t.matchSnapshot(printUrl, 'should open greatest version')
})

t.test('fund using package argument with no browser, using --json option', async t => {
  npm.prefix = t.testdir(maintainerOwnsAllDeps)
  config.json = true

  await fund.exec(['.'])
  t.same(
    JSON.parse(printUrl),
    {
      title: 'individual funding available at the following URL',
      url: 'http://example.com/donate',
    },
    'should open funding url using json output'
  )
  config.json = false
})

t.test('fund using package info fetch from registry', async t => {
  npm.prefix = t.testdir({})

  await fund.exec(['ntl'])
  t.match(
    printUrl,
    /http:\/\/example.com\/pacote/,
    'should open funding url that was loaded from registry manifest'
  )
})

t.test('fund tries to use package info fetch from registry but registry has nothing', async t => {
  npm.prefix = t.testdir({})

  await t.rejects(
    fund.exec(['foo']),
    { code: 'ENOFUND', message: 'No valid funding method available for: foo' },
    'should have no valid funding message'
  )
})

t.test('fund but target module has no funding info', async t => {
  npm.prefix = t.testdir(nestedNoFundingPackages)

  await t.rejects(
    fund.exec(['foo']),
    { code: 'ENOFUND', message: 'No valid funding method available for: foo' },
    'should have no valid funding message'
  )
})

t.test('fund using bad which value', async t => {
  npm.prefix = t.testdir(nestedMultipleFundingPackages)
  config.which = 3

  await t.rejects(
    fund.exec(['bar']),
    {
      code: 'EFUNDNUMBER',
      /* eslint-disable-next-line max-len */
      message: '`npm fund [<@scope>/]<pkg> [--which=fundingSourceNumber]` must be given a positive integer',
    },
    'should have bad which option error message'
  )
  config.which = null
})

t.test('fund pkg missing version number', async t => {
  npm.prefix = t.testdir({
    'package.json': JSON.stringify({
      name: 'foo',
      funding: 'http://example.com/foo',
    }),
  })

  await fund.exec([])
  t.matchSnapshot(result, 'should print name only')
})

t.test('fund a package throws on openUrl', async t => {
  npm.prefix = t.testdir({
    'package.json': JSON.stringify({
      name: 'foo',
      version: '1.0.0',
      funding: 'http://npmjs.org',
    }),
  })

  await t.rejects(fund.exec(['.']), { message: 'ERROR' }, 'should throw unknown error')
})

t.test('fund a package with type and multiple sources', async t => {
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

  await fund.exec(['.'])
  t.matchSnapshot(result, 'should print prompt select message')
})

t.test('fund colors', async t => {
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

  await fund.exec([])
  t.matchSnapshot(result, 'should print output with color info')
  npm.color = false
})

t.test('sub dep with fund info and a parent with no funding info', async t => {
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
          funding: ['http://example.com/c', 'http://example.com/c-other'],
        }),
      },
    },
  })

  await fund.exec([])
  t.matchSnapshot(result, 'should nest sub dep as child of root')
})

t.test('workspaces', async t => {
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
            funding: ['http://example.com/c', 'http://example.com/c-other'],
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

    await fund.execWorkspaces([], ['a'])

    t.matchSnapshot(result, 'should display only filtered workspace name and its deps')

    result = ''

    await fund.execWorkspaces([], ['./packages/a'])

    t.matchSnapshot(result, 'should display only filtered workspace path and its deps')
  })
})
