const cacache = require('cacache')
const chalk = require('chalk')
const fs = require('fs')
const fetch = require('make-fetch-happen')
const table = require('text-table')
const which = require('which')
const pacote = require('pacote')
const { resolve } = require('path')
const semver = require('semver')
const { promisify } = require('util')
const ansiTrim = require('../utils/ansi-trim.js')
const isWindows = require('../utils/is-windows.js')
const ping = require('../utils/ping.js')
const { registry: { default: defaultRegistry } } = require('../utils/config/definitions.js')
const lstat = promisify(fs.lstat)
const readdir = promisify(fs.readdir)
const access = promisify(fs.access)
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

const BaseCommand = require('../base-command.js')
class Doctor extends BaseCommand {
  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get description () {
    return 'Check your npm environment'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get name () {
    return 'doctor'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get params () {
    return ['registry']
  }

  async exec (args) {
    this.npm.log.info('Running checkup')

    // each message is [title, ok, message]
    const messages = []

    const actions = [
      ['npm ping', 'checkPing', []],
      ['npm -v', 'getLatestNpmVersion', []],
      ['node -v', 'getLatestNodejsVersion', []],
      ['npm config get registry', 'checkNpmRegistry', []],
      ['which git', 'getGitPath', []],
      ...(isWindows ? [] : [
        ['Perms check on cached files', 'checkFilesPermission', [this.npm.cache, true, R_OK]],
        ['Perms check on local node_modules', 'checkFilesPermission', [this.npm.localDir, true]],
        ['Perms check on global node_modules', 'checkFilesPermission', [this.npm.globalDir, false]],
        ['Perms check on local bin folder', 'checkFilesPermission', [this.npm.localBin, false, R_OK | W_OK | X_OK]],
        ['Perms check on global bin folder', 'checkFilesPermission', [this.npm.globalBin, false, X_OK]],
      ]),
      ['Verify cache contents', 'verifyCachedFiles', [this.npm.flatOptions.cache]],
      // TODO:
      // - ensure arborist.loadActual() runs without errors and no invalid edges
      // - ensure package-lock.json matches loadActual()
      // - verify loadActual without hidden lock file matches hidden lockfile
      // - verify all local packages have bins linked
    ]

    // Do the actual work
    for (const [msg, fn, args] of actions) {
      const line = [msg]
      try {
        line.push(true, await this[fn](...args))
      } catch (er) {
        line.push(false, er)
      }
      messages.push(line)
    }

    const outHead = ['Check', 'Value', 'Recommendation/Notes']
      .map(!this.npm.color ? h => h : h => chalk.underline(h))
    let allOk = true
    const outBody = messages.map(!this.npm.color
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

    const silent = this.npm.log.levels[this.npm.log.level] >
      this.npm.log.levels.error
    if (!silent) {
      this.npm.output(table(outTable, tableOpts))
      if (!allOk)
        console.error('')
    }
    if (!allOk)
      throw new Error('Some problems found. See above for recommendations.')
  }

  async checkPing () {
    const tracker = this.npm.log.newItem('checkPing', 1)
    tracker.info('checkPing', 'Pinging registry')
    try {
      await ping(this.npm.flatOptions)
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

  async getLatestNpmVersion () {
    const tracker = this.npm.log.newItem('getLatestNpmVersion', 1)
    tracker.info('getLatestNpmVersion', 'Getting npm package information')
    try {
      const latest = (await pacote.manifest('npm@latest', this.npm.flatOptions)).version
      if (semver.gte(this.npm.version, latest))
        return `current: v${this.npm.version}, latest: v${latest}`
      else
        throw `Use npm v${latest}`
    } finally {
      tracker.finish()
    }
  }

  async getLatestNodejsVersion () {
    // XXX get the latest in the current major as well
    const current = process.version
    const currentRange = `^${current}`
    const url = 'https://nodejs.org/dist/index.json'
    const tracker = this.npm.log.newItem('getLatestNodejsVersion', 1)
    tracker.info('getLatestNodejsVersion', 'Getting Node.js release information')
    try {
      const res = await fetch(url, { method: 'GET', ...this.npm.flatOptions })
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

  async checkFilesPermission (root, shouldOwn, mask = null) {
    if (mask === null)
      mask = shouldOwn ? R_OK | W_OK : R_OK

    let ok = true

    const tracker = this.npm.log.newItem(root, 1)

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

  async getGitPath () {
    const tracker = this.npm.log.newItem('getGitPath', 1)
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

  async verifyCachedFiles () {
    const tracker = this.npm.log.newItem('verifyCachedFiles', 1)
    tracker.info('verifyCachedFiles', 'Verifying the npm cache')
    try {
      const stats = await cacache.verify(this.npm.flatOptions.cache)
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

  async checkNpmRegistry () {
    if (this.npm.flatOptions.registry !== defaultRegistry)
      throw `Try \`npm config set registry=${defaultRegistry}\``
    else
      return `using default registry (${defaultRegistry})`
  }
}

module.exports = Doctor
