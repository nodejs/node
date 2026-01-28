const path = require('node:path')
const t = require('tap')
const tar = require('tar')
const pack = require('libnpmpack')
const ssri = require('ssri')
const { readFile } = require('fs/promises')
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

t.test('should getContents of a tarball with only a package.json', async (t) => {
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

t.test('should getContents of a tarball with a node_modules directory included', async (t) => {
  const testDir = t.testdir({
    package: {
      'package.json': JSON.stringify({
        name: 'my-cool-pkg',
        version: '1.0.0',
      }, null, 2),
      node_modules: {
        'bundle-dep': {
          'package.json': JSON.stringify({
            name: 'bundle-dep',
            version: '1.0.0',
          }, null, 2),
        },
      },
    },
  })

  const fileName = path.join(testDir, 'npm-example-v1.tgz')
  await tar.c({
    gzip: true,
    file: fileName,
    C: testDir,
  }, ['package'])

  const tarball = await readFile(fileName)

  const tarballContents = await getContents({
    name: 'my-cool-pkg',
    version: '1.0.0',
  }, tarball)

  const integrity = ssri.fromData(tarball, {
    algorithms: ['sha1', 'sha512'],
  })

  // zlib is nondeterministic
  t.match(tarballContents.shasum, /^[0-9a-f]{40}$/)
  delete tarballContents.shasum

  // assert mode differently according to platform
  if (process.platform === 'win32') {
    tarballContents.files[0].mode = 511
    tarballContents.files[1].mode = 511
    tarballContents.files[2].mode = 511
    tarballContents.files[3].mode = 438
    tarballContents.files[4].mode = 438
  } else {
    tarballContents.files[0].mode = 493
    tarballContents.files[1].mode = 493
    tarballContents.files[2].mode = 493
    tarballContents.files[3].mode = 420
    tarballContents.files[4].mode = 420
  }

  tarballContents.files.forEach((file) => {
    delete file.mode
  })

  t.same(tarballContents, {
    id: 'my-cool-pkg@1.0.0',
    name: 'my-cool-pkg',
    version: '1.0.0',
    size: tarball.length,
    unpackedSize: 97,
    integrity: ssri.parse(integrity.sha512[0]),
    filename: 'my-cool-pkg-1.0.0.tgz',
    files: [
      { path: '', size: 0 },
      { path: 'node_modules/', size: 0 },
      { path: 'node_modules/bundle-dep/', size: 0 },
      { path: 'node_modules/bundle-dep/package.json', size: 48 },
      { path: 'package.json', size: 49 },
    ],
    entryCount: 5,
    bundled: ['bundle-dep'],
  }, 'contents are correct')
  t.end()
})

t.test('should log byte sizes correctly', async (t) => {
  const cases = [
    [0, '0 B', '0B'],
    [1, '1 B', '1B'],
    [10, '10 B', '10B'],
    [999, '999 B', '999B'],
    [1000, '1.0 kB', '1.0kB'],
    [1001, '1.0 kB', '1.0kB'],
    [1500, '1.5 kB', '1.5kB'],
    [999999, '1.0 MB', '1.0MB'],
    [1000000, '1.0 MB', '1.0MB'],
    [999999999, '1.0 GB', '1.0GB'],
    [1000000000, '1.0 GB', '1.0GB'],
  ]

  for (const [size, expected, expectedNoSpace] of cases) {
    const logs = printLogs({
      name: 'pkg',
      version: '1.0.0',
      files: [
        { path: 'file.txt', size: size },
      ],
      bundled: [],
      size: size,
      unpackedSize: size,
      integrity: 'sha512-xxx',
    })

    t.match(logs, `package size: ${expected}`, `package size: ${expected}`)
    t.match(logs, `unpacked size: ${expected}`, `unpacked size: ${expected}`)
    t.match(logs, `${expectedNoSpace} file.txt`, `file size: ${expectedNoSpace}`)
  }
})
