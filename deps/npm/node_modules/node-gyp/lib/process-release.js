var semver = require('semver')
var url = require('url')
var path = require('path')

var bitsre = /(x86|x64)/
var bitsreV3 = /(x86|ia32|x64)/ // io.js v3.x.x shipped with "ia32" but should
                                // have been "x86"

// Captures all the logic required to determine download URLs, local directory and 
// file names and whether we need to use SHA-1 or 2. Inputs come from command-line
// switches (--target), `process.version` and `process.release` where it exists.
function processRelease (argv, gyp, defaultVersion, defaultRelease) {
  var version = argv[0] || gyp.opts.target || defaultVersion

  // parse the version to normalize and ensure it's valid
  var versionSemver = semver.parse(version)
  if (!versionSemver) {
    return { version: version }
  }
  // flatten version into String
  version = versionSemver.version

  var overrideDistUrl = gyp.opts['dist-url'] || gyp.opts.disturl
  var iojs = !overrideDistUrl && !defaultRelease && semver.satisfies(version, '>=1.0.0 <4.0.0')
  var name = (defaultRelease && defaultRelease.name.replace(/\./g, '')) || (iojs ? 'iojs' : 'node') // io.js -> iojs
  var defaultDirUrl = (overrideDistUrl || iojs ? 'https://iojs.org/download/release' : 'https://nodejs.org/dist') + '/v' + version + '/'
  var baseUrl
  var libUrl32
  var libUrl64

  // new style, based on process.release so we have a lot of the data we need
  if (defaultRelease && defaultRelease.headersUrl && !overrideDistUrl) {
    baseUrl = url.resolve(defaultRelease.headersUrl, './')
    libUrl32 = resolveLibUrl(name, defaultRelease.libUrl || baseUrl || defaultDirUrl, 'x86', version)
    libUrl64 = resolveLibUrl(name, defaultRelease.libUrl || baseUrl || defaultDirUrl, 'x64', version)

    return {
      version: version,
      semver: versionSemver,
      name: name,
      baseUrl: baseUrl,
      tarballUrl: defaultRelease.headersUrl,
      shasumsFile: 'SHASUMS256.txt',
      shasumsUrl: url.resolve(baseUrl, 'SHASUMS256.txt'),
      checksumAlgo: 'sha256',
      versionDir: (name !== 'node' ? name + '-' : '') + version,
      libUrl32: libUrl32,
      libUrl64: libUrl64,
      libPath32: normalizePath(path.relative(url.parse(baseUrl).path, url.parse(libUrl32).path)),
      libPath64: normalizePath(path.relative(url.parse(baseUrl).path, url.parse(libUrl64).path))
    }
  }

  // older versions without process.release are captured here and we have to make
  // a lot of assumptions

  // distributions starting with 0.10.0 contain sha256 checksums
  var checksumAlgo = semver.gte(version, '0.10.0') ? 'sha256' : 'sha1'
  var shasumsFile = (checksumAlgo === 'sha256') ? 'SHASUMS256.txt' : 'SHASUMS.txt'

  baseUrl = defaultDirUrl
  libUrl32 = resolveLibUrl(name, baseUrl, 'x86', version)
  libUrl64 = resolveLibUrl(name, baseUrl, 'x64', version)

  return {
    version: version,
    semver: versionSemver,
    name: name,
    baseUrl: baseUrl,
    tarballUrl: baseUrl + name + '-v' + version + '.tar.gz',
    shasumsFile: shasumsFile,
    shasumsUrl: baseUrl + shasumsFile,
    checksumAlgo: checksumAlgo,
    versionDir: (name !== 'node' ? name + '-' : '') + version,
    libUrl32: libUrl32,
    libUrl64: libUrl64,
    libPath32: normalizePath(path.relative(url.parse(baseUrl).path, url.parse(libUrl32).path)),
    libPath64: normalizePath(path.relative(url.parse(baseUrl).path, url.parse(libUrl64).path))
  }
}

function normalizePath (p) {
  return path.normalize(p).replace(/\\/g, '/')
}

function resolveLibUrl (name, defaultUrl, arch, version) {
  var base = url.resolve(defaultUrl, './')
  var isV3 = semver.satisfies(version, '^3')
  var hasLibUrl = bitsre.test(defaultUrl) || (isV3 && bitsreV3.test(defaultUrl))

  if (!hasLibUrl) {
    // let's assume it's a baseUrl then
    if (semver.gte(version, '1.0.0'))
      return url.resolve(base, 'win-' + arch  +'/' + name + '.lib')
    // prior to io.js@1.0.0 32-bit node.lib lives in /, 64-bit lives in /x64/
    return url.resolve(base, (arch == 'x64' ? 'x64/' : '') + name + '.lib')
  }

  // else we have a proper url to a .lib, just make sure it's the right arch
  return defaultUrl.replace(isV3 ? bitsreV3 : bitsre, arch)
}

module.exports = processRelease
