var test = require('tape')
var processRelease = require('../lib/process-release')

test('test process release - process.version = 0.8.20', function (t) {
  t.plan(2)

  var release = processRelease([], { opts: {} }, 'v0.8.20', null)

  t.equal(release.semver.version, '0.8.20')
  delete release.semver

  t.deepEqual(release, {
    version: '0.8.20',
    name: 'node',
    baseUrl: 'https://nodejs.org/dist/v0.8.20/',
    tarballUrl: 'https://nodejs.org/dist/v0.8.20/node-v0.8.20.tar.gz',
    shasumsFile: 'SHASUMS.txt',
    shasumsUrl: 'https://nodejs.org/dist/v0.8.20/SHASUMS.txt',
    checksumAlgo: 'sha1',
    versionDir: '0.8.20',
    libUrl32: 'https://nodejs.org/dist/v0.8.20/node.lib',
    libUrl64: 'https://nodejs.org/dist/v0.8.20/x64/node.lib',
    libPath32: 'node.lib',
    libPath64: 'x64/node.lib'
  })
})

test('test process release - process.version = 0.10.21', function (t) {
  t.plan(2)

  var release = processRelease([], { opts: {} }, 'v0.10.21', null)

  t.equal(release.semver.version, '0.10.21')
  delete release.semver

  t.deepEqual(release, {
    version: '0.10.21',
    name: 'node',
    baseUrl: 'https://nodejs.org/dist/v0.10.21/',
    tarballUrl: 'https://nodejs.org/dist/v0.10.21/node-v0.10.21.tar.gz',
    shasumsFile: 'SHASUMS256.txt',
    shasumsUrl: 'https://nodejs.org/dist/v0.10.21/SHASUMS256.txt',
    checksumAlgo: 'sha256',
    versionDir: '0.10.21',
    libUrl32: 'https://nodejs.org/dist/v0.10.21/node.lib',
    libUrl64: 'https://nodejs.org/dist/v0.10.21/x64/node.lib',
    libPath32: 'node.lib',
    libPath64: 'x64/node.lib'
  })
})

test('test process release - process.version = 0.12.22', function (t) {
  t.plan(2)

  var release = processRelease([], { opts: {} }, 'v0.12.22', null)

  t.equal(release.semver.version, '0.12.22')
  delete release.semver

  t.deepEqual(release, {
    version: '0.12.22',
    name: 'node',
    baseUrl: 'https://nodejs.org/dist/v0.12.22/',
    tarballUrl: 'https://nodejs.org/dist/v0.12.22/node-v0.12.22.tar.gz',
    shasumsFile: 'SHASUMS256.txt',
    shasumsUrl: 'https://nodejs.org/dist/v0.12.22/SHASUMS256.txt',
    checksumAlgo: 'sha256',
    versionDir: '0.12.22',
    libUrl32: 'https://nodejs.org/dist/v0.12.22/node.lib',
    libUrl64: 'https://nodejs.org/dist/v0.12.22/x64/node.lib',
    libPath32: 'node.lib',
    libPath64: 'x64/node.lib'
  })
})

test('test process release - process.release ~ node@4.1.23', function (t) {
  t.plan(2)

  var release = processRelease([], { opts: {} }, 'v4.1.23', {
    name: 'node',
    headersUrl: 'https://nodejs.org/dist/v4.1.23/node-v4.1.23-headers.tar.gz'
  })

  t.equal(release.semver.version, '4.1.23')
  delete release.semver

  t.deepEqual(release, {
    version: '4.1.23',
    name: 'node',
    baseUrl: 'https://nodejs.org/dist/v4.1.23/',
    tarballUrl: 'https://nodejs.org/dist/v4.1.23/node-v4.1.23-headers.tar.gz',
    shasumsFile: 'SHASUMS256.txt',
    shasumsUrl: 'https://nodejs.org/dist/v4.1.23/SHASUMS256.txt',
    checksumAlgo: 'sha256',
    versionDir: '4.1.23',
    libUrl32: 'https://nodejs.org/dist/v4.1.23/win-x86/node.lib',
    libUrl64: 'https://nodejs.org/dist/v4.1.23/win-x64/node.lib',
    libPath32: 'win-x86/node.lib',
    libPath64: 'win-x64/node.lib'
  })
})

test('test process release - process.release ~ node@4.1.23 / corp build', function (t) {
  t.plan(2)

  var release = processRelease([], { opts: {} }, 'v4.1.23', {
    name: 'node',
    headersUrl: 'https://some.custom.location/node-v4.1.23-headers.tar.gz'
  })

  t.equal(release.semver.version, '4.1.23')
  delete release.semver

  t.deepEqual(release, {
    version: '4.1.23',
    name: 'node',
    baseUrl: 'https://some.custom.location/',
    tarballUrl: 'https://some.custom.location/node-v4.1.23-headers.tar.gz',
    shasumsFile: 'SHASUMS256.txt',
    shasumsUrl: 'https://some.custom.location/SHASUMS256.txt',
    checksumAlgo: 'sha256',
    versionDir: '4.1.23',
    libUrl32: 'https://some.custom.location/win-x86/node.lib',
    libUrl64: 'https://some.custom.location/win-x64/node.lib',
    libPath32: 'win-x86/node.lib',
    libPath64: 'win-x64/node.lib'
  })
})

test('test process release - process.version = 1.8.4', function (t) {
  t.plan(2)

  var release = processRelease([], { opts: {} }, 'v1.8.4', null)

  t.equal(release.semver.version, '1.8.4')
  delete release.semver

  t.deepEqual(release, {
    version: '1.8.4',
    name: 'iojs',
    baseUrl: 'https://iojs.org/download/release/v1.8.4/',
    tarballUrl: 'https://iojs.org/download/release/v1.8.4/iojs-v1.8.4.tar.gz',
    shasumsFile: 'SHASUMS256.txt',
    shasumsUrl: 'https://iojs.org/download/release/v1.8.4/SHASUMS256.txt',
    checksumAlgo: 'sha256',
    versionDir: 'iojs-1.8.4',
    libUrl32: 'https://iojs.org/download/release/v1.8.4/win-x86/iojs.lib',
    libUrl64: 'https://iojs.org/download/release/v1.8.4/win-x64/iojs.lib',
    libPath32: 'win-x86/iojs.lib',
    libPath64: 'win-x64/iojs.lib'
  })
})

test('test process release - process.release ~ iojs@3.2.24', function (t) {
  t.plan(2)

  var release = processRelease([], { opts: {} }, 'v3.2.24', {
    name: 'io.js',
    headersUrl: 'https://iojs.org/download/release/v3.2.24/iojs-v3.2.24-headers.tar.gz'
  })

  t.equal(release.semver.version, '3.2.24')
  delete release.semver

  t.deepEqual(release, {
    version: '3.2.24',
    name: 'iojs',
    baseUrl: 'https://iojs.org/download/release/v3.2.24/',
    tarballUrl: 'https://iojs.org/download/release/v3.2.24/iojs-v3.2.24-headers.tar.gz',
    shasumsFile: 'SHASUMS256.txt',
    shasumsUrl: 'https://iojs.org/download/release/v3.2.24/SHASUMS256.txt',
    checksumAlgo: 'sha256',
    versionDir: 'iojs-3.2.24',
    libUrl32: 'https://iojs.org/download/release/v3.2.24/win-x86/iojs.lib',
    libUrl64: 'https://iojs.org/download/release/v3.2.24/win-x64/iojs.lib',
    libPath32: 'win-x86/iojs.lib',
    libPath64: 'win-x64/iojs.lib'
  })
})

test('test process release - process.release ~ iojs@3.2.11 +libUrl32', function (t) {
  t.plan(2)

  var release = processRelease([], { opts: {} }, 'v3.2.11', {
    name: 'io.js',
    headersUrl: 'https://iojs.org/download/release/v3.2.11/iojs-v3.2.11-headers.tar.gz',
    libUrl: 'https://iojs.org/download/release/v3.2.11/libdir-x86/iojs.lib' // custom
  })

  t.equal(release.semver.version, '3.2.11')
  delete release.semver

  t.deepEqual(release, {
    version: '3.2.11',
    name: 'iojs',
    baseUrl: 'https://iojs.org/download/release/v3.2.11/',
    tarballUrl: 'https://iojs.org/download/release/v3.2.11/iojs-v3.2.11-headers.tar.gz',
    shasumsFile: 'SHASUMS256.txt',
    shasumsUrl: 'https://iojs.org/download/release/v3.2.11/SHASUMS256.txt',
    checksumAlgo: 'sha256',
    versionDir: 'iojs-3.2.11',
    libUrl32: 'https://iojs.org/download/release/v3.2.11/libdir-x86/iojs.lib',
    libUrl64: 'https://iojs.org/download/release/v3.2.11/libdir-x64/iojs.lib',
    libPath32: 'libdir-x86/iojs.lib',
    libPath64: 'libdir-x64/iojs.lib'
  })
})

test('test process release - process.release ~ iojs@3.2.101 +libUrl64', function (t) {
  t.plan(2)

  var release = processRelease([], { opts: {} }, 'v3.2.101', {
    name: 'io.js',
    headersUrl: 'https://iojs.org/download/release/v3.2.101/iojs-v3.2.101-headers.tar.gz',
    libUrl: 'https://iojs.org/download/release/v3.2.101/libdir-x64/iojs.lib' // custom
  })

  t.equal(release.semver.version, '3.2.101')
  delete release.semver

  t.deepEqual(release, {
    version: '3.2.101',
    name: 'iojs',
    baseUrl: 'https://iojs.org/download/release/v3.2.101/',
    tarballUrl: 'https://iojs.org/download/release/v3.2.101/iojs-v3.2.101-headers.tar.gz',
    shasumsFile: 'SHASUMS256.txt',
    shasumsUrl: 'https://iojs.org/download/release/v3.2.101/SHASUMS256.txt',
    checksumAlgo: 'sha256',
    versionDir: 'iojs-3.2.101',
    libUrl32: 'https://iojs.org/download/release/v3.2.101/libdir-x86/iojs.lib',
    libUrl64: 'https://iojs.org/download/release/v3.2.101/libdir-x64/iojs.lib',
    libPath32: 'libdir-x86/iojs.lib',
    libPath64: 'libdir-x64/iojs.lib'
  })
})

test('test process release - process.release ~ iojs@3.3.0 - borked libdir', function (t) {
  t.plan(2)

  var release = processRelease([], { opts: {} }, 'v3.2.101', {
    name: 'io.js',
    headersUrl: 'https://iojs.org/download/release/v3.2.101/iojs-v3.2.101-headers.tar.gz',
    libUrl: 'https://iojs.org/download/release/v3.2.101/win-ia32/iojs.lib' // custom
  })

  t.equal(release.semver.version, '3.2.101')
  delete release.semver

  t.deepEqual(release, {
    version: '3.2.101',
    name: 'iojs',
    baseUrl: 'https://iojs.org/download/release/v3.2.101/',
    tarballUrl: 'https://iojs.org/download/release/v3.2.101/iojs-v3.2.101-headers.tar.gz',
    shasumsFile: 'SHASUMS256.txt',
    shasumsUrl: 'https://iojs.org/download/release/v3.2.101/SHASUMS256.txt',
    checksumAlgo: 'sha256',
    versionDir: 'iojs-3.2.101',
    libUrl32: 'https://iojs.org/download/release/v3.2.101/win-x86/iojs.lib',
    libUrl64: 'https://iojs.org/download/release/v3.2.101/win-x64/iojs.lib',
    libPath32: 'win-x86/iojs.lib',
    libPath64: 'win-x64/iojs.lib'
  })
})
