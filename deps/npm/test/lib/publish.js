const t = require('tap')
const requireInject = require('require-inject')

// mock config
const {defaults} = require('../../lib/utils/config.js')
const credentials = {
  'https://unauthed.registry': {
    email: 'me@example.com',
  },
  'https://scope.specific.registry': {
    token: 'some.registry.token',
    alwaysAuth: false,
  },
  'https://some.registry': {
    token: 'some.registry.token',
    alwaysAuth: false,
  },
  'https://registry.npmjs.org/': {
    token: 'npmjs.registry.token',
    alwaysAuth: false,
  },
}
const config = {
  list: [defaults],
}

const registryCredentials = (t, registry) => {
  return (uri) => {
    t.same(uri, registry, 'gets credentials for expected registry')
    return credentials[uri]
  }
}

const fs = require('fs')

t.test('should publish with libnpmpublish, respecting publishConfig', (t) => {
  t.plan(6)

  const registry = 'https://some.registry'
  const publishConfig = { registry }
  const testDir = t.testdir({
    'package.json': JSON.stringify({
      name: 'my-cool-pkg',
      version: '1.0.0',
      publishConfig,
    }, null, 2),
  })

  const publish = requireInject('../../lib/publish.js', {
    '../../lib/npm.js': {
      flatOptions: {
        json: true,
        defaultTag: 'latest',
        registry,
      },
      config: {
        ...config,
        getCredentialsByURI: registryCredentials(t, registry),
      },
    },
    '../../lib/utils/tar.js': {
      getContents: () => ({
        id: 'someid',
      }),
      logTar: () => {},
    },
    '../../lib/utils/output.js': () => {},
    '../../lib/utils/otplease.js': (opts, fn) => {
      return Promise.resolve().then(() => fn(opts))
    },
    // verify that we do NOT remove publishConfig if it was there originally
    // and then removed during the script/pack process
    libnpmpack: async () => {
      fs.writeFileSync(`${testDir}/package.json`, JSON.stringify({
        name: 'my-cool-pkg',
        version: '1.0.0',
      }))
      return Buffer.from('')
    },
    libnpmpublish: {
      publish: (manifest, tarData, opts) => {
        t.match(manifest, { name: 'my-cool-pkg', version: '1.0.0' }, 'gets manifest')
        t.isa(tarData, Buffer, 'tarData is a buffer')
        t.ok(opts, 'gets opts object')
        t.same(opts.registry, publishConfig.registry, 'publishConfig is passed through')
      },
    },
  })

  return publish([testDir], (er) => {
    if (er)
      throw er
    t.pass('got to callback')
  })
})

t.test('re-loads publishConfig if added during script process', (t) => {
  t.plan(6)
  const registry = 'https://some.registry'
  const publishConfig = { registry }
  const testDir = t.testdir({
    'package.json': JSON.stringify({
      name: 'my-cool-pkg',
      version: '1.0.0',
    }, null, 2),
  })

  const publish = requireInject('../../lib/publish.js', {
    '../../lib/npm.js': {
      flatOptions: {
        json: true,
        defaultTag: 'latest',
        registry: 'https://registry.npmjs.org/',
      },
      config: {
        ...config,
        getCredentialsByURI: registryCredentials(t, registry),
      },
    },
    '../../lib/utils/tar.js': {
      getContents: () => ({
        id: 'someid',
      }),
      logTar: () => {},
    },
    '../../lib/utils/output.js': () => {},
    '../../lib/utils/otplease.js': (opts, fn) => {
      return Promise.resolve().then(() => fn(opts))
    },
    libnpmpack: async () => {
      fs.writeFileSync(`${testDir}/package.json`, JSON.stringify({
        name: 'my-cool-pkg',
        version: '1.0.0',
        publishConfig,
      }))
      return Buffer.from('')
    },
    libnpmpublish: {
      publish: (manifest, tarData, opts) => {
        t.match(manifest, { name: 'my-cool-pkg', version: '1.0.0' }, 'gets manifest')
        t.isa(tarData, Buffer, 'tarData is a buffer')
        t.ok(opts, 'gets opts object')
        t.same(opts.registry, registry, 'publishConfig is passed through')
      },
    },
  })

  return publish([testDir], (er) => {
    if (er)
      throw er
    t.pass('got to callback')
  })
})

t.test('should not log if silent (dry run)', (t) => {
  t.plan(2)

  const registry = 'https://registry.npmjs.org'
  const testDir = t.testdir({
    'package.json': JSON.stringify({
      name: 'my-cool-pkg',
      version: '1.0.0',
    }, null, 2),
  })

  const publish = requireInject('../../lib/publish.js', {
    '../../lib/npm.js': {
      flatOptions: {
        json: false,
        defaultTag: 'latest',
        dryRun: true,
        registry,
      },
      config: {
        ...config,
        getCredentialsByURI: () => {
          throw new Error('should not call getCredentialsByURI in dry run')
        },
      },
    },
    '../../lib/utils/tar.js': {
      getContents: () => ({}),
      logTar: () => {
        t.pass('called logTar (but nothing actually printed)')
      },
    },
    '../../lib/utils/otplease.js': (opts, fn) => {
      return Promise.resolve().then(() => fn(opts))
    },
    '../../lib/utils/output.js': () => {
      throw new Error('should not output in silent mode')
    },
    npmlog: {
      verbose: () => {},
      notice: () => {},
      level: 'silent',
    },
    libnpmpack: async () => '',
    libnpmpublish: {
      publish: (manifest, tarData, opts) => {
        throw new Error('should not call libnpmpublish in dry run')
      },
    },
  })

  return publish([testDir], (er) => {
    if (er)
      throw er
    t.pass('got to callback')
  })
})

t.test('should log tarball contents (dry run)', (t) => {
  t.plan(3)

  const registry = 'https://registry.npmjs.org'
  const testDir = t.testdir({
    'package.json': JSON.stringify({
      name: 'my-cool-pkg',
      version: '1.0.0',
    }, null, 2),
  })

  const publish = requireInject('../../lib/publish.js', {
    '../../lib/npm.js': {
      flatOptions: {
        json: false,
        defaultTag: 'latest',
        dryRun: true,
        registry,
      },
      config: {
        ...config,
        getCredentialsByURI: () => {
          throw new Error('should not call getCredentialsByURI in dry run')
        }},
    },
    '../../lib/utils/tar.js': {
      getContents: () => ({
        id: 'someid',
      }),
      logTar: () => {
        t.pass('logTar is called')
      },
    },
    '../../lib/utils/output.js': () => {
      t.pass('output fn is called')
    },
    '../../lib/utils/otplease.js': (opts, fn) => {
      return Promise.resolve().then(() => fn(opts))
    },
    libnpmpack: async () => '',
    libnpmpublish: {
      publish: () => {
        throw new Error('should not call libnpmpublish in dry run')
      },
    },
  })

  return publish([testDir], (er) => {
    if (er)
      throw er
    t.pass('got to callback')
  })
})

t.test('shows usage with wrong set of arguments', (t) => {
  t.plan(1)
  const publish = requireInject('../../lib/publish.js', {
    '../../lib/npm.js': {
      flatOptions: {
        json: false,
        defaultTag: '0.0.13',
        registry: 'https://registry.npmjs.org/',
      },
      config,
    },
  })

  return publish(['a', 'b', 'c'], (er) => t.matchSnapshot(er, 'should print usage'))
})

t.test('throws when invalid tag', (t) => {
  t.plan(1)

  const registry = 'https://registry.npmjs.org'

  const publish = requireInject('../../lib/publish.js', {
    '../../lib/npm.js': {
      flatOptions: {
        json: false,
        defaultTag: '0.0.13',
        registry,
      },
      config,
    },
  })

  return publish([], (err) => {
    t.match(err, {
      message: /Tag name must not be a valid SemVer range: /,
    }, 'throws when tag name is a valid SemVer range')
  })
})

t.test('can publish a tarball', t => {
  t.plan(4)

  const registry = 'https://registry.npmjs.org/'
  const testDir = t.testdir({
    package: {
      'package.json': JSON.stringify({
        name: 'my-cool-tarball',
        version: '1.2.3',
      }),
    },
  })
  const tar = require('tar')
  tar.c({
    cwd: testDir,
    file: `${testDir}/package.tgz`,
    sync: true,
  }, ['package'])

  // no cheating!  read it from the tarball.
  fs.unlinkSync(`${testDir}/package/package.json`)
  fs.rmdirSync(`${testDir}/package`)

  const tarFile = fs.readFileSync(`${testDir}/package.tgz`)
  const publish = requireInject('../../lib/publish.js', {
    '../../lib/npm.js': {
      flatOptions: {
        json: true,
        defaultTag: 'latest',
        registry,
      },
      config: {
        ...config,
        getCredentialsByURI: registryCredentials(t, registry),
      },
    },
    '../../lib/utils/tar.js': {
      getContents: () => ({
        id: 'someid',
      }),
      logTar: () => {},
    },
    '../../lib/utils/output.js': () => {},
    '../../lib/utils/otplease.js': (opts, fn) => {
      return Promise.resolve().then(() => fn(opts))
    },
    libnpmpublish: {
      publish: (manifest, tarData, opts) => {
        t.match(manifest, {
          name: 'my-cool-tarball',
          version: '1.2.3',
        }, 'sent manifest to lib pub')
        t.strictSame(tarData, tarFile, 'sent the tarball data to lib pub')
      },
    },
  })

  return publish([`${testDir}/package.tgz`], (er) => {
    if (er)
      throw er
    t.pass('got to callback')
  })
})

t.test('throw if not logged in', async t => {
  t.plan(2)
  const registry = 'https://unauthed.registry'

  const publish = requireInject('../../lib/publish.js', {
    '../../lib/utils/tar.js': {
      getContents: () => ({
        id: 'someid',
      }),
      logTar: () => {},
    },
    '../../lib/npm.js': {
      flatOptions: {
        json: false,
        registry,
      },
      config: {
        ...config,
        getCredentialsByURI: registryCredentials(t, registry),
      },
    },
  })

  return publish([], (err) => {
    t.match(err, {
      message: 'This command requires you to be logged in.',
      code: 'ENEEDAUTH',
    }, 'throws when not logged in')
  })
})

t.test('read registry only from publishConfig', t => {
  t.plan(4)

  const registry = 'https://some.registry'
  const publishConfig = { registry }
  const testDir = t.testdir({
    'package.json': JSON.stringify({
      name: 'my-cool-pkg',
      version: '1.0.0',
      publishConfig,
    }, null, 2),
  })

  const publish = requireInject('../../lib/publish.js', {
    '../../lib/npm.js': {
      flatOptions: {
        json: false,
      },
      config: {
        ...config,
        getCredentialsByURI: registryCredentials(t, registry),
      },
    },
    '../../lib/utils/tar.js': {
      getContents: () => ({
        id: 'someid',
      }),
      logTar: () => {},
    },
    '../../lib/utils/output.js': () => {},
    libnpmpublish: {
      publish: (manifest, tarData, opts) => {
        t.match(manifest, { name: 'my-cool-pkg', version: '1.0.0' }, 'gets manifest')
        t.same(opts.registry, registry, 'publishConfig is passed through')
      },
    },
  })

  return publish([testDir], (er) => {
    if (er)
      throw er
    t.pass('got to callback')
  })
})

t.test('should check auth for scope specific registry', t => {
  const testDir = t.testdir({
    'package.json': JSON.stringify({
      name: '@npm/my-cool-pkg',
      version: '1.0.0',
    }, null, 2),
  })

  const registry = 'https://scope.specific.registry'
  const publish = requireInject('../../lib/publish.js', {
    '../../lib/npm.js': {
      flatOptions: {
        json: false,
        '@npm:registry': registry,
      },
      config: {
        ...config,
        getCredentialsByURI: registryCredentials(t, registry),
      },
    },
    '../../lib/utils/tar.js': {
      getContents: () => ({
        id: 'someid',
      }),
      logTar: () => {},
    },
    '../../lib/utils/output.js': () => {},
    '../../lib/utils/otplease.js': (opts, fn) => {
      return Promise.resolve().then(() => fn(opts))
    },
    libnpmpublish: {
      publish: () => '',
    },
  })
  return publish([testDir], (er) => {
    if (er)
      throw er
    t.pass('got to callback')
  })
})
