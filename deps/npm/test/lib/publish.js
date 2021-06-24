const t = require('tap')
const { fake: mockNpm } = require('../fixtures/mock-npm')
const fs = require('fs')

// The way we set loglevel is kind of convoluted, and there is no way to affect
// it from these tests, which only interact with lib/publish.js, which assumes
// that the code that is requiring and calling lib/publish.js has already
// taken care of the loglevel
const log = require('npmlog')
log.level = 'silent'

const {definitions} = require('../../lib/utils/config')
const defaults = Object.entries(definitions).reduce((defaults, [key, def]) => {
  defaults[key] = def.default
  return defaults
}, {})

t.afterEach(() => log.level = 'silent')

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

  const Publish = t.mock('../../lib/publish.js', {
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
        t.type(tarData, Buffer, 'tarData is a buffer')
        t.ok(opts, 'gets opts object')
        t.same(opts.customValue, true, 'flatOptions values are passed through')
        t.same(opts.registry, registry, 'publishConfig.registry is passed through')
      },
    },
  })
  const npm = mockNpm({
    flatOptions: {
      customValue: true,
    },
  })
  npm.config.getCredentialsByURI = (uri) => {
    t.same(uri, registry, 'gets credentials for expected registry')
    return { token: 'some.registry.token' }
  }
  const publish = new Publish(npm)

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

  const Publish = t.mock('../../lib/publish.js', {
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
        t.type(tarData, Buffer, 'tarData is a buffer')
        t.ok(opts, 'gets opts object')
        t.same(opts.registry, registry, 'publishConfig.registry is passed through')
      },
    },
  })
  const npm = mockNpm()
  npm.config.getCredentialsByURI = (uri) => {
    t.same(uri, registry, 'gets credentials for expected registry')
    return { token: 'some.registry.token' }
  }
  const publish = new Publish(npm)

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
  const Publish = t.mock('../../lib/publish.js', {
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
  const npm = mockNpm({
    config: { json: true },
    output: () => {
      t.pass('output is called')
    },
  })
  npm.config.getCredentialsByURI = (uri) => {
    t.same(uri, npm.config.get('registry'), 'gets credentials for expected registry')
    return { token: 'some.registry.token' }
  }
  const publish = new Publish(npm)

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
  const Publish = t.mock('../../lib/publish.js', {
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
  const npm = mockNpm({
    config: { 'dry-run': true },
    output: () => {
      throw new Error('should not output in dry run mode')
    },
  })
  npm.config.getCredentialsByURI = () => {
    throw new Error('should not call getCredentialsByURI in dry run')
  }

  const publish = new Publish(npm)

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
  const Publish = t.mock('../../lib/publish.js', {
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
  const npm = mockNpm({
    config: { 'dry-run': true },
    output: () => {
      t.pass('output fn is called')
    },
  })
  npm.config.getCredentialsByURI = () => {
    throw new Error('should not call getCredentialsByURI in dry run')
  }
  const publish = new Publish(npm)

  publish.exec([testDir], (er) => {
    if (er)
      throw er
    t.pass('got to callback')
    t.end()
  })
})

t.test('shows usage with wrong set of arguments', (t) => {
  t.plan(1)
  const Publish = t.mock('../../lib/publish.js')
  const publish = new Publish({})

  publish.exec(['a', 'b', 'c'], (er) => {
    t.matchSnapshot(er, 'should print usage')
    t.end()
  })
})

t.test('throws when invalid tag', (t) => {
  t.plan(1)

  const Publish = t.mock('../../lib/publish.js')
  const npm = mockNpm({
    config: { tag: '0.0.13' },
  })
  const publish = new Publish(npm)

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
  const Publish = t.mock('../../lib/publish.js', {
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
  const npm = mockNpm()
  npm.config.getCredentialsByURI = (uri) => {
    t.same(uri, npm.config.get('registry'), 'gets credentials for expected registry')
    return { token: 'some.registry.token' }
  }
  const publish = new Publish(npm)

  publish.exec([`${testDir}/tarball/package.tgz`], (er) => {
    if (er)
      throw er
    t.pass('got to callback')
    t.end()
  })
})

t.test('should check auth for default registry', t => {
  t.plan(2)
  const Publish = t.mock('../../lib/publish.js')
  const npm = mockNpm()
  npm.config.getCredentialsByURI = (uri) => {
    t.same(uri, npm.config.get('registry'), 'gets credentials for expected registry')
    return {}
  }
  const publish = new Publish(npm)

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
  const Publish = t.mock('../../lib/publish.js')
  const npm = mockNpm({
    flatOptions: { registry },
  })
  npm.config.getCredentialsByURI = (uri) => {
    t.same(uri, registry, 'gets credentials for expected registry')
    return {}
  }
  const publish = new Publish(npm)

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

  const Publish = t.mock('../../lib/publish.js')
  const npm = mockNpm({
    flatOptions: { '@npm:registry': registry },
  })
  npm.config.getCredentialsByURI = (uri) => {
    t.same(uri, registry, 'gets credentials for expected registry')
    return {}
  }
  const publish = new Publish(npm)

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

  const Publish = t.mock('../../lib/publish.js', {
    libnpmpublish: {
      publish: (manifest, tarData, opts) => {
        t.ok(opts, 'gets opts object')
        t.same(opts['@npm:registry'], registry, 'scope specific registry is passed through')
      },
    },
  })
  const npm = mockNpm({
    flatOptions: { '@npm:registry': registry },
  })
  npm.config.getCredentialsByURI = (uri) => {
    t.same(uri, registry, 'gets credentials for expected registry')
    return { token: 'some.registry.token' }
  }
  const publish = new Publish(npm)

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

  const Publish = t.mock('../../lib/publish.js', {
    libnpmpublish: {
      publish: (manifest, tarData, opts) => {
        t.match(manifest, { name: 'my-cool-pkg', version: '1.0.0' }, 'gets manifest')
        t.same(opts.registry, registry, 'publishConfig is passed through')
      },
    },
  })
  const npm = mockNpm()
  npm.config.getCredentialsByURI = (uri) => {
    t.same(uri, registry, 'gets credentials for expected registry')
    return { token: 'some.registry.token' }
  }
  const publish = new Publish(npm)

  publish.exec([testDir], (er) => {
    if (er)
      throw er
    t.pass('got to callback')
    t.end()
  })
})

t.test('able to publish after if encountered multiple configs', t => {
  t.plan(3)

  const registry = 'https://some.registry'
  const tag = 'better-tag'
  const publishConfig = { registry }
  const testDir = t.testdir({
    'package.json': JSON.stringify({
      name: 'my-cool-pkg',
      version: '1.0.0',
      publishConfig,
    }, null, 2),
  })

  const configList = [defaults]
  configList.unshift(Object.assign(Object.create(configList[0]), {
    registry: `https://other.registry`,
    tag: 'some-tag',
  }))
  configList.unshift(Object.assign(Object.create(configList[0]), { tag }))

  const Publish = t.mock('../../lib/publish.js', {
    libnpmpublish: {
      publish: (manifest, tarData, opts) => {
        t.same(opts.defaultTag, tag, 'gets option for expected tag')
      },
    },
  })
  const publish = new Publish({
    // what would be flattened by the configList created above
    flatOptions: {
      defaultTag: 'better-tag',
      registry: 'https://other.registry',
    },
    config: {
      get: key => configList[0][key],
      list: configList,
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

t.test('workspaces', (t) => {
  const testDir = t.testdir({
    'package.json': JSON.stringify({
      name: 'my-cool-pkg',
      version: '1.0.0',
      workspaces: ['workspace-a', 'workspace-b', 'workspace-c'],
    }, null, 2),
    'workspace-a': {
      'package.json': JSON.stringify({
        name: 'workspace-a',
        version: '1.2.3-a',
        repository: 'http://repo.workspace-a/',
      }),
    },
    'workspace-b': {
      'package.json': JSON.stringify({
        name: 'workspace-b',
        version: '1.2.3-n',
        repository: 'https://github.com/npm/workspace-b',
      }),
    },
    'workspace-c': {
      'package.json': JSON.stringify({
        name: 'workspace-n',
        version: '1.2.3-n',
      }),
    },
  })

  const publishes = []
  const outputs = []
  t.beforeEach(() => {
    npm.config.set('json', false)
    outputs.length = 0
    publishes.length = 0
  })
  const Publish = t.mock('../../lib/publish.js', {
    '../../lib/utils/tar.js': {
      getContents: (manifest) => ({
        id: manifest._id,
      }),
      logTar: () => {},
    },
    libnpmpublish: {
      publish: (manifest, tarballData, opts) => {
        publishes.push(manifest)
      },
    },
  })
  const npm = mockNpm({
    output: (o) => {
      outputs.push(o)
    },
  })
  npm.localPrefix = testDir
  npm.config.getCredentialsByURI = (uri) => {
    return { token: 'some.registry.token' }
  }
  const publish = new Publish(npm)

  t.test('all workspaces', (t) => {
    log.level = 'info'
    publish.execWorkspaces([], [], (err) => {
      t.notOk(err)
      t.matchSnapshot(publishes, 'should publish all workspaces')
      t.matchSnapshot(outputs, 'should output all publishes')
      t.end()
    })
  })

  t.test('one workspace', t => {
    log.level = 'info'
    publish.execWorkspaces([], ['workspace-a'], (err) => {
      t.notOk(err)
      t.matchSnapshot(publishes, 'should publish given workspace')
      t.matchSnapshot(outputs, 'should output one publish')
      t.end()
    })
  })

  t.test('invalid workspace', t => {
    publish.execWorkspaces([], ['workspace-x'], (err) => {
      t.match(err, /No workspaces found/)
      t.match(err, /workspace-x/)
      t.end()
    })
  })

  t.test('json', t => {
    log.level = 'info'
    npm.config.set('json', true)
    publish.execWorkspaces([], [], (err) => {
      t.notOk(err)
      t.matchSnapshot(publishes, 'should publish all workspaces')
      t.matchSnapshot(outputs, 'should output all publishes as json')
      t.end()
    })
  })
  t.end()
})

t.test('private workspaces', (t) => {
  const testDir = t.testdir({
    'package.json': JSON.stringify({
      name: 'workspaces-project',
      version: '1.0.0',
      workspaces: ['packages/*'],
    }),
    packages: {
      a: {
        'package.json': JSON.stringify({
          name: '@npmcli/a',
          version: '1.0.0',
          private: true,
        }),
      },
      b: {
        'package.json': JSON.stringify({
          name: '@npmcli/b',
          version: '1.0.0',
        }),
      },
    },
  })

  const publishes = []
  const outputs = []
  t.beforeEach(() => {
    npm.config.set('json', false)
    outputs.length = 0
    publishes.length = 0
  })
  const mocks = {
    '../../lib/utils/tar.js': {
      getContents: (manifest) => ({
        id: manifest._id,
      }),
      logTar: () => {},
    },
    libnpmpublish: {
      publish: (manifest, tarballData, opts) => {
        if (manifest.private) {
          throw Object.assign(
            new Error('private pkg'),
            { code: 'EPRIVATE' }
          )
        }
        publishes.push(manifest)
      },
    },
  }
  const npm = mockNpm({
    output: (o) => {
      outputs.push(o)
    },
  })
  npm.localPrefix = testDir
  npm.config.getCredentialsByURI = (uri) => {
    return { token: 'some.registry.token' }
  }

  t.test('with color', t => {
    const Publish = t.mock('../../lib/publish.js', {
      ...mocks,
      npmlog: {
        notice () {},
        verbose () {},
        warn (title, msg) {
          t.equal(title, 'publish', 'should use publish warn title')
          t.match(
            msg,
            'Skipping workspace \u001b[32m@npmcli/a\u001b[39m, marked as \u001b[1mprivate\u001b[22m',
            'should display skip private workspace warn msg'
          )
        },
      },
    })
    const publish = new Publish(npm)

    npm.color = true
    publish.execWorkspaces([], [], (err) => {
      t.notOk(err)
      t.matchSnapshot(publishes, 'should publish all non-private workspaces')
      t.matchSnapshot(outputs, 'should output all publishes')
      npm.color = false
      t.end()
    })
  })

  t.test('colorless', t => {
    const Publish = t.mock('../../lib/publish.js', {
      ...mocks,
      npmlog: {
        notice () {},
        verbose () {},
        warn (title, msg) {
          t.equal(title, 'publish', 'should use publish warn title')
          t.equal(
            msg,
            'Skipping workspace @npmcli/a, marked as private',
            'should display skip private workspace warn msg'
          )
        },
      },
    })
    const publish = new Publish(npm)

    publish.execWorkspaces([], [], (err) => {
      t.notOk(err)
      t.matchSnapshot(publishes, 'should publish all non-private workspaces')
      t.matchSnapshot(outputs, 'should output all publishes')
      t.end()
    })
  })

  t.test('unexpected error', t => {
    const Publish = t.mock('../../lib/publish.js', {
      ...mocks,
      libnpmpublish: {
        publish: (manifest, tarballData, opts) => {
          if (manifest.private)
            throw new Error('ERR')

          publishes.push(manifest)
        },
      },
      npmlog: {
        notice () {},
        verbose () {},
      },
    })
    const publish = new Publish(npm)

    publish.execWorkspaces([], [], (err) => {
      t.match(
        err,
        /ERR/,
        'should throw unexpected error'
      )
      t.end()
    })
  })

  t.end()
})
