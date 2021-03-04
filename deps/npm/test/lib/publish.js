const t = require('tap')
const requireInject = require('require-inject')
const fs = require('fs')

// The way we set loglevel is kind of convoluted, and there is no way to affect
// it from these tests, which only interact with lib/publish.js, which assumes
// that the code that is requiring and calling lib/publish.js has already
// taken care of the loglevel
const log = require('npmlog')
log.level = 'silent'

// mock config
const {defaults} = require('../../lib/utils/config.js')

const config = {
  list: [defaults],
}

t.afterEach(cb => {
  log.level = 'silent'
  cb()
})

t.test('should publish with libnpmpublish, passing through flatOptions and respecting publishConfig.registry', (t) => {
  t.plan(7)

  const registry = 'https://some.registry'
  const publishConfig = { registry }
  const testDir = t.testdir({
    'package.json': JSON.stringify({
      name: 'my-cool-pkg',
      version: '1.0.0',
      publishConfig,
    }, null, 2),
  })

  const Publish = requireInject('../../lib/publish.js', {
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
        t.same(opts.customValue, true, 'flatOptions values are passed through')
        t.same(opts.registry, registry, 'publishConfig.registry is passed through')
      },
    },
  })
  const publish = new Publish({
    flatOptions: {
      customValue: true,
    },
    config: {
      ...config,
      getCredentialsByURI: (uri) => {
        t.same(uri, registry, 'gets credentials for expected registry')
        return { token: 'some.registry.token' }
      },
    },
  })

  publish.exec([testDir], (er) => {
    if (er)
      throw er
    t.pass('got to callback')
    t.end()
  })
})

t.test('re-loads publishConfig.registry if added during script process', (t) => {
  t.plan(6)
  const registry = 'https://some.registry'
  const publishConfig = { registry }
  const testDir = t.testdir({
    'package.json': JSON.stringify({
      name: 'my-cool-pkg',
      version: '1.0.0',
    }, null, 2),
  })

  const Publish = requireInject('../../lib/publish.js', {
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
        t.same(opts.registry, registry, 'publishConfig.registry is passed through')
      },
    },
  })
  const publish = new Publish({
    config: {
      ...config,
      getCredentialsByURI: (uri) => {
        t.same(uri, registry, 'gets credentials for expected registry')
        return { token: 'some.registry.token' }
      },
    },
  })

  publish.exec([testDir], (er) => {
    if (er)
      throw er
    t.pass('got to callback')
    t.end()
  })
})

t.test('if loglevel=info and json, should not output package contents', (t) => {
  t.plan(4)

  const testDir = t.testdir({
    'package.json': JSON.stringify({
      name: 'my-cool-pkg',
      version: '1.0.0',
    }, null, 2),
  })

  log.level = 'info'
  const Publish = requireInject('../../lib/publish.js', {
    '../../lib/utils/output.js': () => {
      t.pass('output is called')
    },
    '../../lib/utils/tar.js': {
      getContents: () => ({
        id: 'someid',
      }),
      logTar: () => {
        t.pass('logTar is called')
      },
    },
    libnpmpublish: {
      publish: () => {
        t.pass('publish called')
      },
    },
  })
  const publish = new Publish({
    flatOptions: {
      json: true,
    },
    config: {
      ...config,
      getCredentialsByURI: (uri) => {
        t.same(uri, defaults.registry, 'gets credentials for expected registry')
        return { token: 'some.registry.token' }
      },
    },
  })

  publish.exec([testDir], (er) => {
    if (er)
      throw er
    t.pass('got to callback')
    t.end()
  })
})

t.test('if loglevel=silent and dry-run, should not output package contents or publish or validate credentials, should log tarball contents', (t) => {
  t.plan(2)

  const testDir = t.testdir({
    'package.json': JSON.stringify({
      name: 'my-cool-pkg',
      version: '1.0.0',
    }, null, 2),
  })

  log.level = 'silent'
  const Publish = requireInject('../../lib/publish.js', {
    '../../lib/utils/output.js': () => {
      throw new Error('should not output in dry run mode')
    },
    '../../lib/utils/tar.js': {
      getContents: () => ({
        id: 'someid',
      }),
      logTar: () => {
        t.pass('logTar is called')
      },
    },
    libnpmpublish: {
      publish: () => {
        throw new Error('should not call libnpmpublish in dry run')
      },
    },
  })
  const publish = new Publish({
    flatOptions: {
      dryRun: true,
    },
    config: {
      ...config,
      getCredentialsByURI: () => {
        throw new Error('should not call getCredentialsByURI in dry run')
      },
    },
  })

  publish.exec([testDir], (er) => {
    if (er)
      throw er
    t.pass('got to callback')
    t.end()
  })
})

t.test('if loglevel=info and dry-run, should not publish, should log package contents and log tarball contents', (t) => {
  t.plan(3)

  const testDir = t.testdir({
    'package.json': JSON.stringify({
      name: 'my-cool-pkg',
      version: '1.0.0',
    }, null, 2),
  })

  log.level = 'info'
  const Publish = requireInject('../../lib/publish.js', {
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
    libnpmpublish: {
      publish: () => {
        throw new Error('should not call libnpmpublish in dry run')
      },
    },
  })
  const publish = new Publish({
    flatOptions: {
      dryRun: true,
    },
    config: {
      ...config,
      getCredentialsByURI: () => {
        throw new Error('should not call getCredentialsByURI in dry run')
      }},
  })

  publish.exec([testDir], (er) => {
    if (er)
      throw er
    t.pass('got to callback')
    t.end()
  })
})

t.test('shows usage with wrong set of arguments', (t) => {
  t.plan(1)
  const Publish = requireInject('../../lib/publish.js')
  const publish = new Publish({})

  publish.exec(['a', 'b', 'c'], (er) => {
    t.matchSnapshot(er, 'should print usage')
    t.end()
  })
})

t.test('throws when invalid tag', (t) => {
  t.plan(1)

  const Publish = requireInject('../../lib/publish.js')
  const publish = new Publish({
    flatOptions: {
      defaultTag: '0.0.13',
    },
    config,
  })

  publish.exec([], (err) => {
    t.match(err, {
      message: /Tag name must not be a valid SemVer range: /,
    }, 'throws when tag name is a valid SemVer range')
    t.end()
  })
})

t.test('can publish a tarball', t => {
  t.plan(4)

  const testDir = t.testdir({
    tarball: {},
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
    file: `${testDir}/tarball/package.tgz`,
    sync: true,
  }, ['package'])

  const tarFile = fs.readFileSync(`${testDir}/tarball/package.tgz`)
  const Publish = requireInject('../../lib/publish.js', {
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
  const publish = new Publish({
    config: {
      ...config,
      getCredentialsByURI: (uri) => {
        t.same(uri, defaults.registry, 'gets credentials for expected registry')
        return { token: 'some.registry.token' }
      },
    },
  })

  publish.exec([`${testDir}/tarball/package.tgz`], (er) => {
    if (er)
      throw er
    t.pass('got to callback')
    t.end()
  })
})

t.test('should check auth for default registry', t => {
  t.plan(2)
  const Publish = requireInject('../../lib/publish.js')
  const publish = new Publish({
    config: {
      ...config,
      getCredentialsByURI: (uri) => {
        t.same(uri, defaults.registry, 'gets credentials for expected registry')
        return {}
      },
    },
  })

  publish.exec([], (err) => {
    t.match(err, {
      message: 'This command requires you to be logged in.',
      code: 'ENEEDAUTH',
    }, 'throws when not logged in')
    t.end()
  })
})

t.test('should check auth for configured registry', t => {
  t.plan(2)
  const registry = 'https://some.registry'
  const Publish = requireInject('../../lib/publish.js')
  const publish = new Publish({
    flatOptions: {
      registry,
    },
    config: {
      ...config,
      getCredentialsByURI: (uri) => {
        t.same(uri, registry, 'gets credentials for expected registry')
        return {}
      },
    },
  })

  publish.exec([], (err) => {
    t.match(err, {
      message: 'This command requires you to be logged in.',
      code: 'ENEEDAUTH',
    }, 'throws when not logged in')
    t.end()
  })
})

t.test('should check auth for scope specific registry', t => {
  t.plan(2)
  const registry = 'https://some.registry'
  const testDir = t.testdir({
    'package.json': JSON.stringify({
      name: '@npm/my-cool-pkg',
      version: '1.0.0',
    }, null, 2),
  })

  const Publish = requireInject('../../lib/publish.js')
  const publish = new Publish({
    flatOptions: {
      '@npm:registry': registry,
    },
    config: {
      ...config,
      getCredentialsByURI: (uri) => {
        t.same(uri, registry, 'gets credentials for expected registry')
        return {}
      },
    },
  })

  publish.exec([testDir], (err) => {
    t.match(err, {
      message: 'This command requires you to be logged in.',
      code: 'ENEEDAUTH',
    }, 'throws when not logged in')
    t.end()
  })
})

t.test('should use auth for scope specific registry', t => {
  t.plan(4)
  const registry = 'https://some.registry'
  const testDir = t.testdir({
    'package.json': JSON.stringify({
      name: '@npm/my-cool-pkg',
      version: '1.0.0',
    }, null, 2),
  })

  const Publish = requireInject('../../lib/publish.js', {
    libnpmpublish: {
      publish: (manifest, tarData, opts) => {
        t.ok(opts, 'gets opts object')
        t.same(opts['@npm:registry'], registry, 'scope specific registry is passed through')
      },
    },
  })
  const publish = new Publish({
    flatOptions: {
      '@npm:registry': registry,
    },
    config: {
      ...config,
      getCredentialsByURI: (uri) => {
        t.same(uri, registry, 'gets credentials for expected registry')
        return { token: 'some.registry.token' }
      },
    },
  })
  publish.exec([testDir], (er) => {
    if (er)
      throw er
    t.pass('got to callback')
    t.end()
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

  const Publish = requireInject('../../lib/publish.js', {
    libnpmpublish: {
      publish: (manifest, tarData, opts) => {
        t.match(manifest, { name: 'my-cool-pkg', version: '1.0.0' }, 'gets manifest')
        t.same(opts.registry, registry, 'publishConfig is passed through')
      },
    },
  })
  const publish = new Publish({
    config: {
      ...config,
      getCredentialsByURI: (uri) => {
        t.same(uri, registry, 'gets credentials for expected registry')
        return { token: 'some.registry.token' }
      },
    },
  })

  publish.exec([testDir], (er) => {
    if (er)
      throw er
    t.pass('got to callback')
    t.end()
  })
})
