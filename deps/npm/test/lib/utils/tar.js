const t = require('tap')
const pack = require('libnpmpack')
const ssri = require('ssri')
const tmock = require('../../fixtures/tmock')
const { cleanZlib } = require('../../fixtures/clean-snapshot')

const { getContents } = require('../../../lib/utils/tar.js')
t.cleanSnapshot = data => cleanZlib(data)

const mockTar = ({ notice }) => tmock(t, '{LIB}/utils/tar.js', {
  'proc-log': {
    log: {
      notice,
    },
  },
})

const printLogs = (tarball, options) => {
  const logs = []
  const { logTar } = mockTar({
    notice: (...args) => logs.push(...args),
  })
  logTar(tarball, options)
  return logs.join('\n')
}

t.test('should log tarball contents', async (t) => {
  const testDir = t.testdir({
    'package.json': JSON.stringify({
      name: 'my-cool-pkg',
      version: '1.0.0',
      bundleDependencies: [
        'bundle-dep',
      ],
      dependencies: {
        'bundle-dep': '1.0.0',
      },
    }),
    cat: 'meow',
    chai: 'blub',
    dog: 'woof',
    node_modules: {
      'bundle-dep': {
        'package.json': '',
      },
    },
  })

  const tarball = await pack(testDir)
  const tarballContents = await getContents({
    _id: '1',
    name: 'my-cool-pkg',
    version: '1.0.0',
  }, tarball)

  t.matchSnapshot(printLogs(tarballContents))
})

t.test('should log tarball contents of a scoped package', async (t) => {
  const testDir = t.testdir({
    'package.json': JSON.stringify({
      name: '@myscope/my-cool-pkg',
      version: '1.0.0',
      bundleDependencies: [
        'bundle-dep',
      ],
      dependencies: {
        'bundle-dep': '1.0.0',
      },
    }),
    cat: 'meow',
    chai: 'blub',
    dog: 'woof',
    node_modules: {
      'bundle-dep': {
        'package.json': '',
      },
    },
  })

  const tarball = await pack(testDir)
  const tarballContents = await getContents({
    _id: '1',
    name: '@myscope/my-cool-pkg',
    version: '1.0.0',
  }, tarball)

  t.matchSnapshot(printLogs(tarballContents))
})

t.test('should log tarball contents with unicode', async (t) => {
  const { logTar } = mockTar({
    notice: (str) => {
      t.ok(true, 'defaults to proc-log')
      return str
    },
  })

  logTar({
    files: [],
    bundled: [],
    size: 0,
    unpackedSize: 0,
    integrity: '',
  }, { unicode: true })
  t.end()
})

t.test('should getContents of a tarball', async (t) => {
  const testDir = t.testdir({
    'package.json': JSON.stringify({
      name: 'my-cool-pkg',
      version: '1.0.0',
    }, null, 2),
  })

  const tarball = await pack(testDir)

  const tarballContents = await getContents({
    name: 'my-cool-pkg',
    version: '1.0.0',
  }, tarball)

  const integrity = await ssri.fromData(tarball, {
    algorithms: ['sha1', 'sha512'],
  })

  // zlib is nondeterministic
  t.match(tarballContents.shasum, /^[0-9a-f]{40}$/)
  delete tarballContents.shasum
  t.strictSame(tarballContents, {
    id: 'my-cool-pkg@1.0.0',
    name: 'my-cool-pkg',
    version: '1.0.0',
    size: tarball.length,
    unpackedSize: 49,
    integrity: ssri.parse(integrity.sha512[0]),
    filename: 'my-cool-pkg-1.0.0.tgz',
    files: [{ path: 'package.json', size: 49, mode: 420 }],
    entryCount: 1,
    bundled: [],
  }, 'contents are correct')
  t.end()
})
