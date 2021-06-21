const t = require('tap')
let ciMock = null
const flatOptions = { global: false, cache: t.testdir() + '/_cacache' }

const MANIFEST_REQUEST = []
const CURRENT_VERSION = '123.420.69'
const CURRENT_MAJOR = '122.420.69'
const CURRENT_MINOR = '123.419.69'
const CURRENT_PATCH = '123.420.68'
const NEXT_VERSION = '123.421.70'
const NEXT_MINOR = '123.420.70'
const NEXT_PATCH = '123.421.69'
const CURRENT_BETA = '124.0.0-beta.99999'
const HAVE_BETA = '124.0.0-beta.0'

let PACOTE_ERROR = null
const pacote = {
  manifest: async (spec, opts) => {
    if (!spec.match(/^npm@/)) {
      console.error(new Error('should only fetch manifest for npm'))
      process.exit(1)
    }
    MANIFEST_REQUEST.push(spec)
    if (PACOTE_ERROR)
      throw PACOTE_ERROR

    return {
      version: spec === 'npm@latest' ? CURRENT_VERSION
      : /-/.test(spec) ? CURRENT_BETA
      : NEXT_VERSION,
    }
  },
}

const npm = {
  flatOptions,
  log: { useColor: () => true },
  version: CURRENT_VERSION,
  config: { get: (k) => k !== 'global' },
  command: 'view',
  argv: ['npm'],
}
const npmNoColor = {
  ...npm,
  log: { useColor: () => false },
}

const { basename } = require('path')

let STAT_ERROR = null
let STAT_MTIME = null
let WRITE_ERROR = null
const fs = {
  ...require('fs'),
  stat: (path, cb) => {
    if (basename(path) !== '_update-notifier-last-checked') {
      console.error(new Error('should only write to notifier last checked file'))
      process.exit(1)
    }
    process.nextTick(() => cb(STAT_ERROR, { mtime: new Date(STAT_MTIME) }))
  },
  writeFile: (path, content, cb) => {
    if (content !== '') {
      console.error(new Error('should not be writing content'))
      process.exit(1)
    }
    if (basename(path) !== '_update-notifier-last-checked') {
      console.error(new Error('should only write to notifier last checked file'))
      process.exit(1)
    }
    process.nextTick(() => cb(WRITE_ERROR))
  },
}

const updateNotifier = t.mock('../../../lib/utils/update-notifier.js', {
  '@npmcli/ci-detect': () => ciMock,
  pacote,
  fs,
})

t.afterEach(() => {
  MANIFEST_REQUEST.length = 0
  STAT_ERROR = null
  PACOTE_ERROR = null
  STAT_MTIME = null
  WRITE_ERROR = null
})

const runUpdateNotifier = async npm => {
  await updateNotifier(npm)
  return npm.updateNotification
}

t.test('situations in which we do not notify', t => {
  t.test('nothing to do if notifier disabled', async t => {
    t.equal(await runUpdateNotifier({
      ...npm,
      config: { get: (k) => k !== 'update-notifier' },
    }), null)
    t.strictSame(MANIFEST_REQUEST, [], 'no requests for manifests')
  })

  t.test('do not suggest update if already updating', async t => {
    t.equal(await runUpdateNotifier({
      ...npm,
      flatOptions: { ...flatOptions, global: true },
      command: 'install',
      argv: ['npm'],
    }), null)
    t.strictSame(MANIFEST_REQUEST, [], 'no requests for manifests')
  })

  t.test('do not suggest update if already updating with spec', async t => {
    t.equal(await runUpdateNotifier({
      ...npm,
      flatOptions: { ...flatOptions, global: true },
      command: 'install',
      argv: ['npm@latest'],
    }), null)
    t.strictSame(MANIFEST_REQUEST, [], 'no requests for manifests')
  })

  t.test('do not update if same as latest', async t => {
    t.equal(await runUpdateNotifier(npm), null)
    t.strictSame(MANIFEST_REQUEST, ['npm@latest'], 'requested latest version')
  })
  t.test('check if stat errors (here for coverage)', async t => {
    STAT_ERROR = new Error('blorg')
    t.equal(await runUpdateNotifier(npm), null)
    t.strictSame(MANIFEST_REQUEST, ['npm@latest'], 'requested latest version')
  })
  t.test('ok if write errors (here for coverage)', async t => {
    WRITE_ERROR = new Error('grolb')
    t.equal(await runUpdateNotifier(npm), null)
    t.strictSame(MANIFEST_REQUEST, ['npm@latest'], 'requested latest version')
  })
  t.test('ignore pacote failures (here for coverage)', async t => {
    PACOTE_ERROR = new Error('pah-KO-tchay')
    t.equal(await runUpdateNotifier(npm), null)
    t.strictSame(MANIFEST_REQUEST, ['npm@latest'], 'requested latest version')
  })
  t.test('do not update if newer than latest, but same as next', async t => {
    t.equal(await runUpdateNotifier({ ...npm, version: NEXT_VERSION }), null)
    const reqs = ['npm@latest', `npm@^${NEXT_VERSION}`]
    t.strictSame(MANIFEST_REQUEST, reqs, 'requested latest and next versions')
  })
  t.test('do not update if on the latest beta', async t => {
    t.equal(await runUpdateNotifier({ ...npm, version: CURRENT_BETA }), null)
    const reqs = [`npm@^${CURRENT_BETA}`]
    t.strictSame(MANIFEST_REQUEST, reqs, 'requested latest and next versions')
  })

  t.test('do not update in CI', async t => {
    t.teardown(() => {
      ciMock = null
    })
    ciMock = 'something'
    t.equal(await runUpdateNotifier(npm), null)
    t.strictSame(MANIFEST_REQUEST, [], 'no requests for manifests')
  })

  t.test('only check weekly for GA releases', async t => {
    // One week (plus five minutes to account for test environment fuzziness)
    STAT_MTIME = Date.now() - (1000 * 60 * 60 * 24 * 7) + (1000 * 60 * 5)
    t.equal(await runUpdateNotifier(npm), null)
    t.strictSame(MANIFEST_REQUEST, [], 'no requests for manifests')
  })

  t.test('only check daily for betas', async t => {
    // One day (plus five minutes to account for test environment fuzziness)
    STAT_MTIME = Date.now() - (1000 * 60 * 60 * 24) + (1000 * 60 * 5)
    t.equal(await runUpdateNotifier({ ...npm, version: HAVE_BETA }), null)
    t.strictSame(MANIFEST_REQUEST, [], 'no requests for manifests')
  })

  t.end()
})

t.test('notification situations', t => {
  t.test('new beta available', async t => {
    const version = HAVE_BETA
    t.matchSnapshot(await runUpdateNotifier({ ...npm, version }), 'color')
    t.matchSnapshot(await runUpdateNotifier({ ...npmNoColor, version }), 'no color')
    t.strictSame(MANIFEST_REQUEST, [`npm@^${version}`, `npm@^${version}`])
  })

  t.test('patch to next version', async t => {
    const version = NEXT_PATCH
    t.matchSnapshot(await runUpdateNotifier({ ...npm, version }), 'color')
    t.matchSnapshot(await runUpdateNotifier({ ...npmNoColor, version }), 'no color')
    t.strictSame(MANIFEST_REQUEST, ['npm@latest', `npm@^${version}`, 'npm@latest', `npm@^${version}`])
  })

  t.test('minor to next version', async t => {
    const version = NEXT_MINOR
    t.matchSnapshot(await runUpdateNotifier({ ...npm, version }), 'color')
    t.matchSnapshot(await runUpdateNotifier({ ...npmNoColor, version }), 'no color')
    t.strictSame(MANIFEST_REQUEST, ['npm@latest', `npm@^${version}`, 'npm@latest', `npm@^${version}`])
  })

  t.test('patch to current', async t => {
    const version = CURRENT_PATCH
    t.matchSnapshot(await runUpdateNotifier({ ...npm, version }), 'color')
    t.matchSnapshot(await runUpdateNotifier({ ...npmNoColor, version }), 'no color')
    t.strictSame(MANIFEST_REQUEST, ['npm@latest', 'npm@latest'])
  })

  t.test('minor to current', async t => {
    const version = CURRENT_MINOR
    t.matchSnapshot(await runUpdateNotifier({ ...npm, version }), 'color')
    t.matchSnapshot(await runUpdateNotifier({ ...npmNoColor, version }), 'no color')
    t.strictSame(MANIFEST_REQUEST, ['npm@latest', 'npm@latest'])
  })

  t.test('major to current', async t => {
    const version = CURRENT_MAJOR
    t.matchSnapshot(await runUpdateNotifier({ ...npm, version }), 'color')
    t.matchSnapshot(await runUpdateNotifier({ ...npmNoColor, version }), 'no color')
    t.strictSame(MANIFEST_REQUEST, ['npm@latest', 'npm@latest'])
  })

  t.end()
})
