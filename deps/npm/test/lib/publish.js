const t = require('tap')
const requireInject = require('require-inject')

// mock config
const {defaults} = require('../../lib/utils/config.js')
const credentials = {
  token: 'asdfasdf',
  alwaysAuth: false,
}
const config = {
  list: [defaults],
  getCredentialsByURI: () => credentials,
}
const fs = require('fs')

t.test('should publish with libnpmpublish, respecting publishConfig', (t) => {
  t.plan(5)

  const publishConfig = { registry: 'https://some.registry' }
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
        registry: 'https://registry.npmjs.org',
      },
      config,
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
  t.plan(5)
  const publishConfig = { registry: 'https://some.registry' }
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
      config,
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

t.test('should not log if silent', (t) => {
  t.plan(2)

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
        registry: 'https://registry.npmjs.org/',
      },
      config,
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
        throw new Error('should not call libnpmpublish!')
      },
    },
  })

  return publish([testDir], (er) => {
    if (er)
      throw er
    t.pass('got to callback')
  })
})

t.test('should log tarball contents', (t) => {
  t.plan(3)
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
        registry: 'https://registry.npmjs.org/',
      },
      config: {
        ...config,
        getCredentialsByURI: () => {
          throw new Error('should not call getCredentialsByURI!')
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
        throw new Error('should not call libnpmpublish!')
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

  return publish([], (err) => {
    t.match(err, {
      message: /Tag name must not be a valid SemVer range: /,
    }, 'throws when tag name is a valid SemVer range')
  })
})

t.test('can publish a tarball', t => {
  t.plan(3)
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
        registry: 'https://registry.npmjs.org/',
      },
      config,
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

t.test('throw if no registry', async t => {
  t.plan(1)
  const publish = requireInject('../../lib/publish.js', {
    '../../lib/npm.js': {
      flatOptions: {
        json: false,
        defaultTag: '0.0.13',
        registry: null,
      },
      config,
    },
  })

  return publish([], (err) => {
    t.match(err, {
      message: 'No registry specified.',
      code: 'ENOREGISTRY',
    }, 'throws when registry unset')
  })
})

t.test('throw if not logged in', async t => {
  t.plan(1)
  const publish = requireInject('../../lib/publish.js', {
    '../../lib/npm.js': {
      flatOptions: {
        json: false,
        defaultTag: '0.0.13',
        registry: 'https://registry.npmjs.org/',
      },
      config: {
        ...config,
        getCredentialsByURI: () => ({
          email: 'me@example.com',
        }),
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
