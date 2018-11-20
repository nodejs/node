'use strict'

// npm pack <pkg>
// Packs the specified package into a .tgz file, which can then
// be installed.

const BB = require('bluebird')

const byteSize = require('byte-size')
const cacache = require('cacache')
const columnify = require('columnify')
const cp = require('child_process')
const deprCheck = require('./utils/depr-check')
const fpm = require('./fetch-package-metadata')
const fs = require('graceful-fs')
const install = require('./install')
const lifecycle = BB.promisify(require('./utils/lifecycle'))
const log = require('npmlog')
const move = require('move-concurrently')
const npm = require('./npm')
const output = require('./utils/output')
const pacote = require('pacote')
const pacoteOpts = require('./config/pacote')
const path = require('path')
const PassThrough = require('stream').PassThrough
const pathIsInside = require('path-is-inside')
const pipe = BB.promisify(require('mississippi').pipe)
const prepublishWarning = require('./utils/warn-deprecated')('prepublish-on-install')
const pinflight = require('promise-inflight')
const readJson = BB.promisify(require('read-package-json'))
const tar = require('tar')
const packlist = require('npm-packlist')
const ssri = require('ssri')

pack.usage = 'npm pack [[<@scope>/]<pkg>...] [--dry-run]'

// if it can be installed, it can be packed.
pack.completion = install.completion

module.exports = pack
function pack (args, silent, cb) {
  const cwd = process.cwd()
  if (typeof cb !== 'function') {
    cb = silent
    silent = false
  }

  if (args.length === 0) args = ['.']

  BB.all(
    args.map((arg) => pack_(arg, cwd))
  ).then((tarballs) => {
    if (!silent && npm.config.get('json')) {
      output(JSON.stringify(tarballs, null, 2))
    } else if (!silent) {
      tarballs.forEach(logContents)
      output(tarballs.map((f) => path.relative(cwd, f.filename)).join('\n'))
    }
    return tarballs
  }).nodeify(cb)
}

function pack_ (pkg, dir) {
  return BB.fromNode((cb) => fpm(pkg, dir, cb)).then((mani) => {
    let name = mani.name[0] === '@'
    // scoped packages get special treatment
      ? mani.name.substr(1).replace(/\//g, '-')
      : mani.name
    const target = `${name}-${mani.version}.tgz`
    return pinflight(target, () => {
      const dryRun = npm.config.get('dry-run')
      if (mani._requested.type === 'directory') {
        return prepareDirectory(mani._resolved)
          .then(() => {
            return packDirectory(mani, mani._resolved, target, target, true, dryRun)
          })
      } else if (dryRun) {
        log.verbose('pack', '--dry-run mode enabled. Skipping write.')
        return cacache.tmp.withTmp(npm.tmp, {tmpPrefix: 'packing'}, (tmp) => {
          const tmpTarget = path.join(tmp, path.basename(target))
          return packFromPackage(pkg, tmpTarget, target)
        })
      } else {
        return packFromPackage(pkg, target, target)
      }
    })
  })
}

function packFromPackage (arg, target, filename) {
  const opts = pacoteOpts()
  return pacote.tarball.toFile(arg, target, pacoteOpts())
    .then(() => cacache.tmp.withTmp(npm.tmp, {tmpPrefix: 'unpacking'}, (tmp) => {
      const tmpTarget = path.join(tmp, filename)
      return pacote.extract(arg, tmpTarget, opts)
        .then(() => readJson(path.join(tmpTarget, 'package.json')))
    }))
    .then((pkg) => getContents(pkg, target, filename))
}

module.exports.prepareDirectory = prepareDirectory
function prepareDirectory (dir) {
  return readJson(path.join(dir, 'package.json')).then((pkg) => {
    if (!pkg.name) {
      throw new Error('package.json requires a "name" field')
    }
    if (!pkg.version) {
      throw new Error('package.json requires a valid "version" field')
    }
    if (!pathIsInside(dir, npm.tmp)) {
      if (pkg.scripts && pkg.scripts.prepublish) {
        prepublishWarning([
          'As of npm@5, `prepublish` scripts are deprecated.',
          'Use `prepare` for build steps and `prepublishOnly` for upload-only.',
          'See the deprecation note in `npm help scripts` for more information.'
        ])
      }
      if (npm.config.get('ignore-prepublish')) {
        return lifecycle(pkg, 'prepare', dir).then(() => pkg)
      } else {
        return lifecycle(pkg, 'prepublish', dir).then(() => {
          return lifecycle(pkg, 'prepare', dir)
        }).then(() => pkg)
      }
    }
    return pkg
  })
}

module.exports.packDirectory = packDirectory
function packDirectory (mani, dir, target, filename, logIt, dryRun) {
  deprCheck(mani)
  return readJson(path.join(dir, 'package.json')).then((pkg) => {
    return lifecycle(pkg, 'prepack', dir)
  }).then(() => {
    return readJson(path.join(dir, 'package.json'))
  }).then((pkg) => {
    return cacache.tmp.withTmp(npm.tmp, {tmpPrefix: 'packing'}, (tmp) => {
      const tmpTarget = path.join(tmp, path.basename(target))

      const tarOpt = {
        file: tmpTarget,
        cwd: dir,
        prefix: 'package/',
        portable: true,
        // Provide a specific date in the 1980s for the benefit of zip,
        // which is confounded by files dated at the Unix epoch 0.
        mtime: new Date('1985-10-26T08:15:00.000Z'),
        gzip: true
      }

      return BB.resolve(packlist({ path: dir }))
      // NOTE: node-tar does some Magic Stuff depending on prefixes for files
      //       specifically with @ signs, so we just neutralize that one
      //       and any such future "features" by prepending `./`
        .then((files) => tar.create(tarOpt, files.map((f) => `./${f}`)))
        .then(() => getContents(pkg, tmpTarget, filename, logIt))
        // thread the content info through
        .tap(() => {
          if (dryRun) {
            log.verbose('pack', '--dry-run mode enabled. Skipping write.')
          } else {
            return move(tmpTarget, target, {Promise: BB, fs})
          }
        })
        .tap(() => lifecycle(pkg, 'postpack', dir))
    })
  })
}

module.exports.logContents = logContents
function logContents (tarball) {
  log.notice('')
  log.notice('', `${npm.config.get('unicode') ? '📦 ' : 'package:'} ${tarball.name}@${tarball.version}`)
  log.notice('=== Tarball Contents ===')
  if (tarball.files.length) {
    log.notice('', columnify(tarball.files.map((f) => {
      const bytes = byteSize(f.size)
      return {path: f.path, size: `${bytes.value}${bytes.unit}`}
    }), {
      include: ['size', 'path'],
      showHeaders: false
    }))
  }
  if (tarball.bundled.length) {
    log.notice('=== Bundled Dependencies ===')
    tarball.bundled.forEach((name) => log.notice('', name))
  }
  log.notice('=== Tarball Details ===')
  log.notice('', columnify([
    {name: 'name:', value: tarball.name},
    {name: 'version:', value: tarball.version},
    tarball.filename && {name: 'filename:', value: tarball.filename},
    {name: 'package size:', value: byteSize(tarball.size)},
    {name: 'unpacked size:', value: byteSize(tarball.unpackedSize)},
    {name: 'shasum:', value: tarball.shasum},
    {
      name: 'integrity:',
      value: tarball.integrity.toString().substr(0, 20) + '[...]' + tarball.integrity.toString().substr(80)},
    tarball.bundled.length && {name: 'bundled deps:', value: tarball.bundled.length},
    tarball.bundled.length && {name: 'bundled files:', value: tarball.entryCount - tarball.files.length},
    tarball.bundled.length && {name: 'own files:', value: tarball.files.length},
    {name: 'total files:', value: tarball.entryCount}
  ].filter((x) => x), {
    include: ['name', 'value'],
    showHeaders: false
  }))
  log.notice('', '')
}

module.exports.getContents = getContents
function getContents (pkg, target, filename, silent) {
  const bundledWanted = new Set(
    pkg.bundleDependencies ||
    pkg.bundledDependencies ||
    []
  )
  const files = []
  const bundled = new Set()
  let totalEntries = 0
  let totalEntrySize = 0
  return tar.t({
    file: target,
    onentry (entry) {
      totalEntries++
      totalEntrySize += entry.size
      const p = entry.path
      if (p.startsWith('package/node_modules/')) {
        const name = p.match(/^package\/node_modules\/((?:@[^/]+\/)?[^/]+)/)[1]
        if (bundledWanted.has(name)) {
          bundled.add(name)
        }
      } else {
        files.push({
          path: entry.path.replace(/^package\//, ''),
          size: entry.size,
          mode: entry.mode
        })
      }
    },
    strip: 1
  })
    .then(() => BB.all([
      BB.fromNode((cb) => fs.stat(target, cb)),
      ssri.fromStream(fs.createReadStream(target), {
        algorithms: ['sha1', 'sha512']
      })
    ]))
    .then(([stat, integrity]) => {
      const shasum = integrity['sha1'][0].hexDigest()
      return {
        id: pkg._id,
        name: pkg.name,
        version: pkg.version,
        from: pkg._from,
        size: stat.size,
        unpackedSize: totalEntrySize,
        shasum,
        integrity: ssri.parse(integrity['sha512'][0]),
        filename,
        files,
        entryCount: totalEntries,
        bundled: Array.from(bundled)
      }
    })
}

const PASSTHROUGH_OPTS = [
  'always-auth',
  'auth-type',
  'ca',
  'cafile',
  'cert',
  'git',
  'local-address',
  'maxsockets',
  'offline',
  'prefer-offline',
  'prefer-online',
  'proxy',
  'https-proxy',
  'registry',
  'send-metrics',
  'sso-poll-frequency',
  'sso-type',
  'strict-ssl'
]

module.exports.packGitDep = packGitDep
function packGitDep (manifest, dir) {
  const stream = new PassThrough()
  readJson(path.join(dir, 'package.json')).then((pkg) => {
    if (pkg.scripts && pkg.scripts.prepare) {
      log.verbose('prepareGitDep', `${manifest._spec}: installing devDeps and running prepare script.`)
      const cliArgs = PASSTHROUGH_OPTS.reduce((acc, opt) => {
        if (npm.config.get(opt, 'cli') != null) {
          acc.push(`--${opt}=${npm.config.get(opt)}`)
        }
        return acc
      }, [])
      const child = cp.spawn(process.env.NODE || process.execPath, [
        require.resolve('../bin/npm-cli.js'),
        'install',
        '--dev',
        '--prod',
        '--ignore-prepublish',
        '--no-progress',
        '--no-save'
      ].concat(cliArgs), {
        cwd: dir,
        env: process.env
      })
      let errData = []
      let errDataLen = 0
      let outData = []
      let outDataLen = 0
      child.stdout.on('data', (data) => {
        outData.push(data)
        outDataLen += data.length
        log.gauge.pulse('preparing git package')
      })
      child.stderr.on('data', (data) => {
        errData.push(data)
        errDataLen += data.length
        log.gauge.pulse('preparing git package')
      })
      return BB.fromNode((cb) => {
        child.on('error', cb)
        child.on('exit', (code, signal) => {
          if (code > 0) {
            const err = new Error(`${signal}: npm exited with code ${code} while attempting to build ${manifest._requested}. Clone the repository manually and run 'npm install' in it for more information.`)
            err.code = code
            err.signal = signal
            cb(err)
          } else {
            cb()
          }
        })
      }).then(() => {
        if (outDataLen > 0) log.silly('prepareGitDep', '1>', Buffer.concat(outData, outDataLen).toString())
        if (errDataLen > 0) log.silly('prepareGitDep', '2>', Buffer.concat(errData, errDataLen).toString())
      }, (err) => {
        if (outDataLen > 0) log.error('prepareGitDep', '1>', Buffer.concat(outData, outDataLen).toString())
        if (errDataLen > 0) log.error('prepareGitDep', '2>', Buffer.concat(errData, errDataLen).toString())
        throw err
      })
    }
  }).then(() => {
    return readJson(path.join(dir, 'package.json'))
  }).then((pkg) => {
    return cacache.tmp.withTmp(npm.tmp, {
      tmpPrefix: 'pacote-packing'
    }, (tmp) => {
      const tmpTar = path.join(tmp, 'package.tgz')
      return packDirectory(manifest, dir, tmpTar).then(() => {
        return pipe(fs.createReadStream(tmpTar), stream)
      })
    })
  }).catch((err) => stream.emit('error', err))
  return stream
}
