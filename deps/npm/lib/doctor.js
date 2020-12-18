const npm = require('./npm.js')

const chalk = require('chalk')
const ansiTrim = require('./utils/ansi-trim.js')
const table = require('text-table')
const output = require('./utils/output.js')
const completion = require('./utils/completion/none.js')
const usageUtil = require('./utils/usage.js')
const usage = usageUtil('doctor', 'npm doctor')
const { resolve } = require('path')

const ping = require('./utils/ping.js')
const checkPing = async () => {
  const tracker = npm.log.newItem('checkPing', 1)
  tracker.info('checkPing', 'Pinging registry')
  try {
    await ping(npm.flatOptions)
    return ''
  } catch (er) {
    if (/^E\d{3}$/.test(er.code || ''))
      throw er.code.substr(1) + ' ' + er.message
    else
      throw er.message
  } finally {
    tracker.finish()
  }
}

const pacote = require('pacote')
const getLatestNpmVersion = async () => {
  const tracker = npm.log.newItem('getLatestNpmVersion', 1)
  tracker.info('getLatestNpmVersion', 'Getting npm package information')
  try {
    const latest = (await pacote.manifest('npm@latest', npm.flatOptions)).version
    if (semver.gte(npm.version, latest))
      return `current: v${npm.version}, latest: v${latest}`
    else
      throw `Use npm v${latest}`
  } finally {
    tracker.finish()
  }
}

const semver = require('semver')
const fetch = require('make-fetch-happen')
const getLatestNodejsVersion = async () => {
  // XXX get the latest in the current major as well
  const current = process.version
  const currentRange = `^${current}`
  const url = 'https://nodejs.org/dist/index.json'
  const tracker = npm.log.newItem('getLatestNodejsVersion', 1)
  tracker.info('getLatestNodejsVersion', 'Getting Node.js release information')
  try {
    const res = await fetch(url, { method: 'GET', ...npm.flatOptions })
    const data = await res.json()
    let maxCurrent = '0.0.0'
    let maxLTS = '0.0.0'
    for (const { lts, version } of data) {
      if (lts && semver.gt(version, maxLTS))
        maxLTS = version

      if (semver.satisfies(version, currentRange) &&
          semver.gt(version, maxCurrent))
        maxCurrent = version
    }
    const recommended = semver.gt(maxCurrent, maxLTS) ? maxCurrent : maxLTS
    if (semver.gte(process.version, recommended))
      return `current: ${current}, recommended: ${recommended}`
    else
      throw `Use node ${recommended} (current: ${current})`
  } finally {
    tracker.finish()
  }
}

const { promisify } = require('util')
const fs = require('fs')
const { R_OK, W_OK, X_OK } = fs.constants
const maskLabel = mask => {
  const label = []
  if (mask & R_OK)
    label.push('readable')

  if (mask & W_OK)
    label.push('writable')

  if (mask & X_OK)
    label.push('executable')

  return label.join(', ')
}
const lstat = promisify(fs.lstat)
const readdir = promisify(fs.readdir)
const access = promisify(fs.access)
const isWindows = require('./utils/is-windows.js')
const checkFilesPermission = async (root, shouldOwn, mask = null) => {
  if (mask === null)
    mask = shouldOwn ? R_OK | W_OK : R_OK

  let ok = true

  const tracker = npm.log.newItem(root, 1)

  try {
    const uid = process.getuid()
    const gid = process.getgid()
    const files = new Set([root])
    for (const f of files) {
      tracker.silly('checkFilesPermission', f.substr(root.length + 1))
      const st = await lstat(f)
        .catch(er => {
          ok = false
          tracker.warn('checkFilesPermission', 'error getting info for ' + f)
        })

      tracker.completeWork(1)

      if (!st)
        continue

      if (shouldOwn && (uid !== st.uid || gid !== st.gid)) {
        tracker.warn('checkFilesPermission', 'should be owner of ' + f)
        ok = false
      }

      if (!st.isDirectory() && !st.isFile())
        continue

      try {
        await access(f, mask)
      } catch (er) {
        ok = false
        const msg = `Missing permissions on ${f} (expect: ${maskLabel(mask)})`
        tracker.error('checkFilesPermission', msg)
        continue
      }

      if (st.isDirectory()) {
        const entries = await readdir(f)
          .catch(er => {
            ok = false
            tracker.warn('checkFilesPermission', 'error reading directory ' + f)
            return []
          })
        for (const entry of entries)
          files.add(resolve(f, entry))
      }
    }
  } finally {
    tracker.finish()
    if (!ok) {
      throw `Check the permissions of files in ${root}` +
        (shouldOwn ? ' (should be owned by current user)' : '')
    } else
      return ''
  }
}

const which = require('which')
const getGitPath = async () => {
  const tracker = npm.log.newItem('getGitPath', 1)
  tracker.info('getGitPath', 'Finding git in your PATH')
  try {
    return await which('git').catch(er => {
      tracker.warn(er)
      throw "Install git and ensure it's in your PATH."
    })
  } finally {
    tracker.finish()
  }
}

const cacache = require('cacache')
const verifyCachedFiles = async () => {
  const tracker = npm.log.newItem('verifyCachedFiles', 1)
  tracker.info('verifyCachedFiles', 'Verifying the npm cache')
  try {
    const stats = await cacache.verify(npm.flatOptions.cache)
    const {
      badContentCount,
      reclaimedCount,
      missingContent,
      reclaimedSize,
    } = stats
    if (badContentCount || reclaimedCount || missingContent) {
      if (badContentCount)
        tracker.warn('verifyCachedFiles', `Corrupted content removed: ${badContentCount}`)

      if (reclaimedCount)
        tracker.warn('verifyCachedFiles', `Content garbage-collected: ${reclaimedCount} (${reclaimedSize} bytes)`)

      if (missingContent)
        tracker.warn('verifyCachedFiles', `Missing content: ${missingContent}`)

      tracker.warn('verifyCachedFiles', 'Cache issues have been fixed')
    }
    tracker.info('verifyCachedFiles', `Verification complete. Stats: ${
      JSON.stringify(stats, null, 2)
    }`)
    return `verified ${stats.verifiedContent} tarballs`
  } finally {
    tracker.finish()
  }
}

const { defaults: { registry: defaultRegistry } } = require('./utils/config.js')
const checkNpmRegistry = async () => {
  if (npm.flatOptions.registry !== defaultRegistry)
    throw `Try \`npm config set registry=${defaultRegistry}\``
  else
    return `using default registry (${defaultRegistry})`
}

const cmd = (args, cb) => doctor(args).then(() => cb()).catch(cb)

const doctor = async args => {
  npm.log.info('Running checkup')

  // each message is [title, ok, message]
  const messages = []

  const actions = [
    ['npm ping', checkPing, []],
    ['npm -v', getLatestNpmVersion, []],
    ['node -v', getLatestNodejsVersion, []],
    ['npm config get registry', checkNpmRegistry, []],
    ['which git', getGitPath, []],
    ...(isWindows ? [] : [
      ['Perms check on cached files', checkFilesPermission, [npm.cache, true, R_OK]],
      ['Perms check on local node_modules', checkFilesPermission, [npm.localDir, true]],
      ['Perms check on global node_modules', checkFilesPermission, [npm.globalDir, false]],
      ['Perms check on local bin folder', checkFilesPermission, [npm.localBin, false, R_OK | W_OK | X_OK]],
      ['Perms check on global bin folder', checkFilesPermission, [npm.globalBin, false, X_OK]],
    ]),
    ['Verify cache contents', verifyCachedFiles, [npm.flatOptions.cache]],
    // TODO:
    // - ensure arborist.loadActual() runs without errors and no invalid edges
    // - ensure package-lock.json matches loadActual()
    // - verify loadActual without hidden lock file matches hidden lockfile
    // - verify all local packages have bins linked
  ]

  for (const [msg, fn, args] of actions) {
    const line = [msg]
    try {
      line.push(true, await fn(...args))
    } catch (er) {
      line.push(false, er)
    }
    messages.push(line)
  }

  const silent = npm.log.levels[npm.log.level] > npm.log.levels.error

  const outHead = ['Check', 'Value', 'Recommendation/Notes']
    .map(!npm.color ? h => h : h => chalk.underline(h))
  let allOk = true
  const outBody = messages.map(!npm.color
    ? item => {
      allOk = allOk && item[1]
      item[1] = item[1] ? 'ok' : 'not ok'
      item[2] = String(item[2])
      return item
    }
    : item => {
      allOk = allOk && item[1]
      if (!item[1]) {
        item[0] = chalk.red(item[0])
        item[2] = chalk.magenta(String(item[2]))
      }
      item[1] = item[1] ? chalk.green('ok') : chalk.red('not ok')
      return item
    })
  const outTable = [outHead, ...outBody]
  const tableOpts = {
    stringLength: s => ansiTrim(s).length,
  }

  if (!silent) {
    output(table(outTable, tableOpts))
    if (!allOk)
      console.error('')
  }
  if (!allOk)
    throw 'Some problems found. See above for recommendations.'
}

module.exports = Object.assign(cmd, { completion, usage })
