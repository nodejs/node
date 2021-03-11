const t = require('tap')
const requireInject = require('require-inject')

let result = ''
const noop = () => null
const npm = {
  localPrefix: '',
  flatOptions: {
    force: false,
    silent: false,
    loglevel: 'silly',
  },
  output: (...msg) => {
    result += msg.join('\n')
  },
}
const mocks = {
  npmlog: { silly () {}, verbose () {} },
  libnpmaccess: { lsPackages: noop },
  libnpmpublish: { unpublish: noop },
  'npm-package-arg': noop,
  'npm-registry-fetch': { json: noop },
  'read-package-json': cb => cb(),
  '../../lib/utils/otplease.js': async (opts, fn) => fn(opts),
  '../../lib/utils/usage.js': () => 'usage instructions',
  '../../lib/utils/get-identity.js': async () => 'foo',
}

t.afterEach(cb => {
  result = ''
  npm.flatOptions.force = false
  npm.flatOptions.loglevel = 'silly'
  npm.flatOptions.silent = false
  cb()
})

t.test('no args --force', t => {
  t.plan(9)

  npm.flatOptions.force = true

  const npmlog = {
    silly (title) {
      t.equal(title, 'unpublish', 'should silly log args')
    },
    verbose (title, msg) {
      t.equal(title, 'unpublish', 'should have expected title')
      t.match(
        msg,
        { name: 'pkg', version: '1.0.0' },
        'should have msg printing package.json contents'
      )
    },
  }

  const npa = {
    resolve (name, version) {
      t.equal(name, 'pkg', 'should npa.resolve package name')
      t.equal(version, '1.0.0', 'should npa.resolve package version')
      return 'pkg@1.0.0'
    },
  }

  const libnpmpublish = {
    unpublish (spec, opts) {
      t.equal(spec, 'pkg@1.0.0', 'should unpublish expected spec')
      t.deepEqual(
        opts,
        {
          force: true,
          silent: false,
          loglevel: 'silly',
          publishConfig: undefined,
        },
        'should unpublish with expected opts'
      )
    },
  }

  const Unpublish = requireInject('../../lib/unpublish.js', {
    ...mocks,
    npmlog,
    libnpmpublish,
    'npm-package-arg': npa,
    'read-package-json': (path, cb) => cb(null, {
      name: 'pkg',
      version: '1.0.0',
    }),
  })
  const unpublish = new Unpublish(npm)

  unpublish.exec([], err => {
    if (err)
      throw err

    t.equal(
      result,
      '- pkg@1.0.0',
      'should output removed pkg@version on success'
    )
  })
})

t.test('no args --force missing package.json', t => {
  npm.flatOptions.force = true

  const Unpublish = requireInject('../../lib/unpublish.js', {
    ...mocks,
    'read-package-json': (path, cb) => cb(Object.assign(
      new Error('ENOENT'),
      { code: 'ENOENT' }
    )),
  })
  const unpublish = new Unpublish(npm)

  unpublish.exec([], err => {
    t.match(
      err,
      /usage instructions/,
      'should throw usage instructions on missing package.json'
    )
    t.end()
  })
})

t.test('no args --force unknown error reading package.json', t => {
  npm.flatOptions.force = true

  const Unpublish = requireInject('../../lib/unpublish.js', {
    ...mocks,
    'read-package-json': (path, cb) => cb(new Error('ERR')),
  })
  const unpublish = new Unpublish(npm)

  unpublish.exec([], err => {
    t.match(
      err,
      /ERR/,
      'should throw unknown error from reading package.json'
    )
    t.end()
  })
})

t.test('no args', t => {
  const Unpublish = requireInject('../../lib/unpublish.js', {
    ...mocks,
  })
  const unpublish = new Unpublish(npm)

  unpublish.exec([], err => {
    t.match(
      err,
      /Refusing to delete entire project/,
      'should throw --force required error on no args'
    )
    t.end()
  })
})

t.test('too many args', t => {
  const Unpublish = requireInject('../../lib/unpublish.js', {
    ...mocks,
  })
  const unpublish = new Unpublish(npm)

  unpublish.exec(['a', 'b'], err => {
    t.match(
      err,
      /usage instructions/,
      'should throw usage instructions if too many args'
    )
    t.end()
  })
})

t.test('unpublish <pkg>@version', t => {
  t.plan(7)

  const pa = {
    name: 'pkg',
    rawSpec: '1.0.0',
    type: 'version',
  }

  const npmlog = {
    silly (title, key, value) {
      t.equal(title, 'unpublish', 'should silly log args')
      if (key === 'spec')
        t.equal(value, pa, 'should log parsed npa object')
      else
        t.equal(value, 'pkg@1.0.0', 'should log originally passed arg')
    },
  }

  const npa = () => pa

  const libnpmpublish = {
    unpublish (spec, opts) {
      t.equal(spec, pa, 'should unpublish expected parsed spec')
      t.deepEqual(
        opts,
        {
          force: false,
          silent: false,
          loglevel: 'silly',
        },
        'should unpublish with expected opts'
      )
    },
  }

  const Unpublish = requireInject('../../lib/unpublish.js', {
    ...mocks,
    npmlog,
    libnpmpublish,
    'npm-package-arg': npa,
  })
  const unpublish = new Unpublish(npm)

  unpublish.exec(['pkg@1.0.0'], err => {
    if (err)
      throw err

    t.equal(
      result,
      '- pkg@1.0.0',
      'should output removed pkg@version on success'
    )
  })
})

t.test('no version found in package.json', t => {
  npm.flatOptions.force = true

  const npa = () => ({
    name: 'pkg',
    type: 'version',
  })

  npa.resolve = () => ''

  const Unpublish = requireInject('../../lib/unpublish.js', {
    ...mocks,
    'npm-package-arg': npa,
    'read-package-json': (path, cb) => cb(null, {
      name: 'pkg',
    }),
  })
  const unpublish = new Unpublish(npm)

  unpublish.exec([], err => {
    if (err)
      throw err

    t.equal(
      result,
      '- pkg',
      'should output removed pkg on success'
    )
    t.end()
  })
})

t.test('unpublish <pkg> --force no version set', t => {
  npm.flatOptions.force = true

  const Unpublish = requireInject('../../lib/unpublish.js', {
    ...mocks,
    'npm-package-arg': () => ({
      name: 'pkg',
      rawSpec: '',
      type: 'tag',
    }),
  })
  const unpublish = new Unpublish(npm)

  unpublish.exec(['pkg'], err => {
    if (err)
      throw err

    t.equal(
      result,
      '- pkg',
      'should output pkg removed'
    )
    t.end()
  })
})

t.test('silent', t => {
  npm.flatOptions.loglevel = 'silent'

  const npa = () => ({
    name: 'pkg',
    rawSpec: '1.0.0',
    type: 'version',
  })

  npa.resolve = () => ''

  const Unpublish = requireInject('../../lib/unpublish.js', {
    ...mocks,
    'npm-package-arg': npa,
  })
  const unpublish = new Unpublish(npm)

  unpublish.exec(['pkg@1.0.0'], err => {
    if (err)
      throw err

    t.equal(
      result,
      '',
      'should have no output'
    )
    t.end()
  })
})

t.test('completion', async t => {
  const testComp =
    async (t, { unpublish, argv, partialWord, expect, title }) => {
      const res = await unpublish.completion(
        {conf: {argv: {remain: argv}}, partialWord}
      )
      t.strictSame(res, expect, title || argv.join(' '))
    }

  t.test('completing with multiple versions from the registry', async t => {
    const Unpublish = requireInject('../../lib/unpublish.js', {
      ...mocks,
      libnpmaccess: {
        async lsPackages () {
          return {
            pkg: 'write',
            bar: 'write',
          }
        },
      },
      'npm-package-arg': require('npm-package-arg'),
      'npm-registry-fetch': {
        async json () {
          return {
            versions: {
              '1.0.0': {},
              '1.0.1': {},
              '2.0.0': {},
            },
          }
        },
      },
    })
    const unpublish = new Unpublish(npm)

    await testComp(t, {
      unpublish,
      argv: ['npm', 'unpublish'],
      partialWord: 'pkg',
      expect: [
        'pkg@1.0.0',
        'pkg@1.0.1',
        'pkg@2.0.0',
      ],
    })
  })

  t.test('no versions retrieved', async t => {
    const Unpublish = requireInject('../../lib/unpublish.js', {
      ...mocks,
      libnpmaccess: {
        async lsPackages () {
          return {
            pkg: 'write',
            bar: 'write',
          }
        },
      },
      'npm-package-arg': require('npm-package-arg'),
      'npm-registry-fetch': {
        async json () {
          return {
            versions: {},
          }
        },
      },
    })
    const unpublish = new Unpublish(npm)

    await testComp(t, {
      unpublish,
      argv: ['npm', 'unpublish'],
      partialWord: 'pkg',
      expect: [
        'pkg',
      ],
      title: 'should autocomplete package name only',
    })
  })

  t.test('packages starting with same letters', async t => {
    const Unpublish = requireInject('../../lib/unpublish.js', {
      ...mocks,
      libnpmaccess: {
        async lsPackages () {
          return {
            pkg: 'write',
            pkga: 'write',
            pkgb: 'write',
          }
        },
      },
      'npm-package-arg': require('npm-package-arg'),
    })
    const unpublish = new Unpublish(npm)

    await testComp(t, {
      unpublish,
      argv: ['npm', 'unpublish'],
      partialWord: 'pkg',
      expect: [
        'pkg',
        'pkga',
        'pkgb',
      ],
    })
  })

  t.test('no packages retrieved', async t => {
    const Unpublish = requireInject('../../lib/unpublish.js', {
      ...mocks,
      libnpmaccess: {
        async lsPackages () {
          return {}
        },
      },
    })
    const unpublish = new Unpublish(npm)

    await testComp(t, {
      unpublish,
      argv: ['npm', 'unpublish'],
      partialWord: 'pkg',
      expect: [],
      title: 'should have no autocompletion',
    })
  })

  t.test('no pkg name to complete', async t => {
    const Unpublish = requireInject('../../lib/unpublish.js', {
      ...mocks,
      libnpmaccess: {
        async lsPackages () {
          return {
            pkg: {},
            bar: {},
          }
        },
      },
    })
    const unpublish = new Unpublish(npm)

    await testComp(t, {
      unpublish,
      argv: ['npm', 'unpublish'],
      partialWord: undefined,
      expect: ['pkg', 'bar'],
      title: 'should autocomplete with available package names from user',
    })
  })

  t.test('no pkg names retrieved from user account', async t => {
    const Unpublish = requireInject('../../lib/unpublish.js', {
      ...mocks,
      libnpmaccess: {
        async lsPackages () {
          return null
        },
      },
    })
    const unpublish = new Unpublish(npm)

    await testComp(t, {
      unpublish,
      argv: ['npm', 'unpublish'],
      partialWord: 'pkg',
      expect: [],
      title: 'should have no autocomplete',
    })
  })

  t.test('logged out user', async t => {
    const Unpublish = requireInject('../../lib/unpublish.js', {
      ...mocks,
      '../../lib/utils/get-identity.js': () => Promise.reject(new Error('ERR')),
    })
    const unpublish = new Unpublish(npm)

    await testComp(t, {
      unpublish,
      argv: ['npm', 'unpublish'],
      partialWord: 'pkg',
      expect: [],
    })
  })

  t.test('too many args', async t => {
    const Unpublish = requireInject('../../lib/unpublish.js', mocks)
    const unpublish = new Unpublish(npm)

    await testComp(t, {
      unpublish,
      argv: ['npm', 'unpublish', 'foo'],
      partialWord: undefined,
      expect: [],
    })
  })

  t.end()
})
