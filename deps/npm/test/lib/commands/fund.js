const t = require('tap')
const mockNpm = require('../../fixtures/mock-npm')

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

const setup = async (t, { openUrl, ...opts } = {}) => {
  const openedUrls = []

  const res = await mockNpm(t, {
    ...opts,
    mocks: {
      '@npmcli/promise-spawn': { open: openUrl || (async url => openedUrls.push(url)) },
      pacote: {
        manifest: arg =>
          arg.name === 'ntl'
            ? Promise.resolve({ funding: 'http://example.com/pacote' })
            : Promise.reject(new Error('ERROR')),
      },
      ...opts.mocks,
    },
  })

  return {
    ...res,
    openedUrls: () => openedUrls,
    fund: (...args) => res.npm.exec('fund', args),
  }
}

t.test('fund with no package containing funding', async t => {
  const { fund, joinedOutput } = await setup(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: 'no-funding-package',
        version: '0.0.0',
      }),
    },
    config: {},
  })

  await fund()
  t.matchSnapshot(joinedOutput(), 'should print empty funding info')
})

t.test('fund in which same maintainer owns all its deps', async t => {
  const { fund, joinedOutput } = await setup(t, {
    prefixDir: maintainerOwnsAllDeps,
    config: {},
  })

  await fund()
  t.matchSnapshot(joinedOutput(), 'should print stack packages together')
})

t.test('fund in which same maintainer owns all its deps, using --json option', async t => {
  const { fund, joinedOutput } = await setup(t, {
    prefixDir: maintainerOwnsAllDeps,
    config: { json: true },
  })

  await fund()
  t.same(
    JSON.parse(joinedOutput()),
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
})

t.test('fund containing multi-level nested deps with no funding', async t => {
  const { fund, joinedOutput } = await setup(t, {
    prefixDir: nestedNoFundingPackages,
    config: {},
  })

  await fund()
  t.matchSnapshot(joinedOutput(), 'should omit dependencies with no funding declared')
})

t.test('fund containing multi-level nested deps with no funding, using --json option', async t => {
  const { fund, joinedOutput } = await setup(t, {
    prefixDir: nestedNoFundingPackages,
    config: { json: true },
  })

  await fund()
  t.same(
    JSON.parse(joinedOutput()),
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
})

t.test('fund containing multi-level nested deps with no funding, using --json option', async t => {
  const { fund, joinedOutput } = await setup(t, {
    prefixDir: nestedMultipleFundingPackages,
    config: { json: true },
  })

  await fund()
  t.same(
    JSON.parse(joinedOutput()),
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
})

t.test('fund does not support global', async t => {
  const { fund } = await setup(t, {
    config: { global: true },
  })

  await t.rejects(fund(), { code: 'EFUNDGLOBAL' }, 'should throw EFUNDGLOBAL error')
})

t.test('fund using package argument', async t => {
  const { fund, openedUrls, joinedOutput } = await setup(t, {
    prefixDir: maintainerOwnsAllDeps,
    config: {},
  })

  await fund('.')
  t.equal(joinedOutput(), '')
  t.strictSame(openedUrls(), ['http://example.com/donate'], 'should open funding url')
})

t.test('fund does not support global, using --json option', async t => {
  const { fund } = await setup(t, {
    prefixDir: {},
    config: { global: true, json: true },
  })

  await t.rejects(
    fund(),
    { code: 'EFUNDGLOBAL', message: '`npm fund` does not support global packages' },
    'should use expected error msg'
  )
})

t.test('fund using string shorthand', async t => {
  const { fund, openedUrls } = await setup(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: 'funding-string-shorthand',
        version: '0.0.0',
        funding: 'https://example.com/sponsor',
      }),
    },
    config: {},
  })

  await fund('.')
  t.strictSame(openedUrls(), ['https://example.com/sponsor'], 'should open string-only url')
})

t.test('fund using nested packages with multiple sources', async t => {
  const { fund, joinedOutput } = await setup(t, {
    prefixDir: nestedMultipleFundingPackages,
    config: {},
  })

  await fund('.')
  t.matchSnapshot(joinedOutput(), 'should prompt with all available URLs')
})

t.test('fund using symlink ref', async t => {
  const f = 'http://example.com/a'
  const { fund, openedUrls } = await setup(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: 'using-symlink-ref',
        version: '1.0.0',
      }),
      a: {
        'package.json': JSON.stringify({
          name: 'a',
          version: '1.0.0',
          funding: f,
        }),
      },
      node_modules: {
        a: t.fixture('symlink', '../a'),
      },
    },
    config: {},
  })

  // using symlinked ref
  await fund('./node_modules/a')
  t.strictSame(openedUrls(), [f], 'should retrieve funding url from symlink')

  // using target ref
  await fund('./a')
  t.strictSame(openedUrls(), [f, f], 'should retrieve funding url from symlink target')
})

t.test('fund using data from actual tree', async t => {
  const { fund, openedUrls } = await setup(t, {
    prefixDir: {
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
    },
    config: {},
  })

  // using symlinked ref
  await fund('a')
  t.strictSame(
    openedUrls(),
    ['http://example.com/_AAA'],
    'should retrieve fund info from actual tree, using greatest version found'
  )
})

t.test('fund using nested packages with multiple sources, with a source number', async t => {
  const { fund, openedUrls } = await setup(t, {
    prefixDir: nestedMultipleFundingPackages,
    config: { which: '1' },
  })

  await fund('.')
  t.strictSame(openedUrls(), ['https://one.example.com'], 'should open the numbered URL')
})

t.test('fund using pkg name while having conflicting versions', async t => {
  const { fund, openedUrls } = await setup(t, {
    prefixDir: conflictingFundingPackages,
    config: { which: '1' },
  })

  await fund('foo')
  t.strictSame(openedUrls(), ['http://example.com/2'], 'should open greatest version')
})

t.test('fund using bad which value: index too high', async t => {
  const { fund, joinedOutput } = await setup(t, {
    prefixDir: nestedMultipleFundingPackages,
    config: { which: '100' },
  })

  await fund('foo')
  t.match(joinedOutput(), 'not a valid index')
  t.matchSnapshot(joinedOutput(), 'should print message about invalid which')
})

t.test('fund using package argument with no browser, using --json option', async t => {
  const { fund, openedUrls, joinedOutput } = await setup(t, {
    prefixDir: maintainerOwnsAllDeps,
    config: { json: true },
  })

  await fund('.')
  t.equal(joinedOutput(), '', 'no output')
  t.same(
    openedUrls(),
    ['http://example.com/donate'],
    'should open funding url using json output'
  )
})

t.test('fund using package info fetch from registry', async t => {
  const { fund, openedUrls } = await setup(t, {
    prefixDir: {},
    config: {},
  })

  await fund('ntl')
  t.match(
    openedUrls(),
    /http:\/\/example.com\/pacote/,
    'should open funding url that was loaded from registry manifest'
  )
})

t.test('fund tries to use package info fetch from registry but registry has nothing', async t => {
  const { fund } = await setup(t, {
    prefixDir: {},
    config: {},
  })

  await t.rejects(
    fund('foo'),
    { code: 'ENOFUND', message: 'No valid funding method available for: foo' },
    'should have no valid funding message'
  )
})

t.test('fund but target module has no funding info', async t => {
  const { fund } = await setup(t, {
    prefixDir: nestedNoFundingPackages,
    config: {},
  })

  await t.rejects(
    fund('foo'),
    { code: 'ENOFUND', message: 'No valid funding method available for: foo' },
    'should have no valid funding message'
  )
})

t.test('fund using bad which value', async t => {
  const { fund } = await setup(t, {
    prefixDir: nestedMultipleFundingPackages,
    config: { which: '0' },
  })

  await t.rejects(
    fund('bar'),
    {
      code: 'EFUNDNUMBER',
      message: /must be given a positive integer/,
    },
    'should have bad which option error message'
  )
})

t.test('fund pkg missing version number', async t => {
  const { fund, joinedOutput } = await setup(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: 'foo',
        funding: 'http://example.com/foo',
      }),
    },
    config: {},
  })

  await fund()
  t.matchSnapshot(joinedOutput(), 'should print name only')
})

t.test('fund a package throws on openUrl', async t => {
  const { fund } = await setup(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: 'foo',
        version: '1.0.0',
        funding: 'http://npmjs.org',
      }),
    },
    config: {},
    openUrl: () => {
      throw new Error('ERROR')
    },
  })

  await t.rejects(fund('.'), { message: 'ERROR' }, 'should throw unknown error')
})

t.test('fund a package with type and multiple sources', async t => {
  const { fund, joinedOutput } = await setup(t, {
    prefixDir: {
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
    },
    config: {},
  })

  await fund('.')
  t.matchSnapshot(joinedOutput(), 'should print prompt select message')
})

t.test('fund colors', async t => {
  const { fund, joinedOutput } = await setup(t, {
    prefixDir: {
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
    },
    config: { color: 'always' },
  })

  await fund()
  t.matchSnapshot(joinedOutput(), 'should print output with color info')
})

t.test('sub dep with fund info and a parent with no funding info', async t => {
  const { fund, joinedOutput } = await setup(t, {
    prefixDir: {
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
    },
    config: {},
  })

  await fund()
  t.matchSnapshot(joinedOutput(), 'should nest sub dep as child of root')
})

t.test('workspaces', async t => {
  const wsPrefixDir = {
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
  }

  t.test('filter funding info by a specific workspace name', async t => {
    const { fund, joinedOutput } = await setup(t, {
      prefixDir: wsPrefixDir,
      config: {
        workspace: 'a',
      },
    })

    await fund()
    t.matchSnapshot(joinedOutput(), 'should display only filtered workspace name and its deps')
  })

  t.test('filter funding info by a specific workspace path', async t => {
    const { fund, joinedOutput } = await setup(t, {
      prefixDir: wsPrefixDir,
      config: {
        workspace: './packages/a',
      },
    })

    await fund()
    t.matchSnapshot(joinedOutput(), 'should display only filtered workspace name and its deps')
  })
})
