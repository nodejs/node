const t = require('tap')
const pack = require('libnpmpack')
const ssri = require('ssri')
const tmock = require('../../fixtures/tmock')

const { getContents } = require('../../../lib/utils/tar.js')

const mockTar = ({ notice }) => tmock(t, '{LIB}/utils/tar.js', {
  'proc-log': {
    notice,
  },
})

const printLogs = (tarball, options) => {
  const logs = []
  const { logTar } = mockTar({
    notice: (...args) => args.map(el => logs.push(el)),
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

  t.strictSame(tarballContents, {
    id: 'my-cool-pkg@1.0.0',
    name: 'my-cool-pkg',
    version: '1.0.0',
    size: 146,
    unpackedSize: 49,
    shasum: 'b8379c5e69693cdda73aec3d81dae1d11c1e75bd',
    integrity: ssri.parse(integrity.sha512[0]),
    filename: 'my-cool-pkg-1.0.0.tgz',
    files: [{ path: 'package.json', size: 49, mode: 420 }],
    entryCount: 1,
    bundled: [],
  }, 'contents are correct')
  t.end()
})
