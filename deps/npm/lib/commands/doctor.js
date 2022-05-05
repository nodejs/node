const cacache = require('cacache')
const fs = require('fs')
const fetch = require('make-fetch-happen')
const table = require('text-table')
const which = require('which')
const pacote = require('pacote')
const { resolve } = require('path')
const semver = require('semver')
const { promisify } = require('util')
const log = require('../utils/log-shim.js')
const ansiTrim = require('../utils/ansi-trim.js')
const ping = require('../utils/ping.js')
const {
  registry: { default: defaultRegistry },
} = require('../utils/config/definitions.js')
const lstat = promisify(fs.lstat)
const readdir = promisify(fs.readdir)
const access = promisify(fs.access)
const { R_OK, W_OK, X_OK } = fs.constants
const maskLabel = mask => {
  const label = []
  if (mask & R_OK) {
    label.push('readable')
  }

  if (mask & W_OK) {
    label.push('writable')
  }

  if (mask & X_OK) {
    label.push('executable')
  }

  return label.join(', ')
}

const BaseCommand = require('../base-command.js')
class Doctor extends BaseCommand {
  static description = 'Check your npm environment'
  static name = 'doctor'
  static params = ['registry']
  static ignoreImplicitWorkspace = false

  async exec (args) {
    log.info('Running checkup')

    // each message is [title, ok, message]
    const messages = []

    const actions = [
      ['npm ping', 'checkPing', []],
      ['npm -v', 'getLatestNpmVersion', []],
      ['node -v', 'getLatestNodejsVersion', []],
      ['npm config get registry', 'checkNpmRegistry', []],
      ['which git', 'getGitPath', []],
      ...(process.platform === 'win32'
        ? []
        : [
          [
            'Perms check on cached files',
            'checkFilesPermission',
            [this.npm.cache, true, R_OK],
          ], [
            'Perms check on local node_modules',
            'checkFilesPermission',
            [this.npm.localDir, true, R_OK | W_OK, true],
          ], [
            'Perms check on global node_modules',
            'checkFilesPermission',
            [this.npm.globalDir, false, R_OK],
          ], [
            'Perms check on local bin folder',
            'checkFilesPermission',
            [this.npm.localBin, false, R_OK | W_OK | X_OK, true],
          ], [
            'Perms check on global bin folder',
            'checkFilesPermission',
            [this.npm.globalBin, false, X_OK],
          ],
        ]),
      [
        'Verify cache contents',
        'verifyCachedFiles',
        [this.npm.flatOptions.cache],
      ],
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

    const outHead = ['Check', 'Value', 'Recommendation/Notes'].map(h => this.npm.chalk.underline(h))
    let allOk = true
    const outBody = messages.map(item => {
      if (!item[1]) {
        allOk = false
        item[0] = this.npm.chalk.red(item[0])
        item[1] = this.npm.chalk.red('not ok')
        item[2] = this.npm.chalk.magenta(String(item[2]))
      } else {
        item[1] = this.npm.chalk.green('ok')
      }
      return item
    })
    const outTable = [outHead, ...outBody]
    const tableOpts = {
      stringLength: s => ansiTrim(s).length,
    }

    if (!this.npm.silent) {
      this.npm.output(table(outTable, tableOpts))
    }
    if (!allOk) {
      throw new Error('Some problems found. See above for recommendations.')
    }
  }

  async checkPing () {
    const tracker = log.newItem('checkPing', 1)
    tracker.info('checkPing', 'Pinging registry')
    try {
      await ping({ ...this.npm.flatOptions, retry: false })
      return ''
    } catch (er) {
      if (/^E\d{3}$/.test(er.code || '')) {
        throw er.code.slice(1) + ' ' + er.message
      } else {
        throw er.message
      }
    } finally {
      tracker.finish()
    }
  }

  async getLatestNpmVersion () {
    const tracker = log.newItem('getLatestNpmVersion', 1)
    tracker.info('getLatestNpmVersion', 'Getting npm package information')
    try {
      const latest = (await pacote.manifest('npm@latest', this.npm.flatOptions)).version
      if (semver.gte(this.npm.version, latest)) {
        return `current: v${this.npm.version}, latest: v${latest}`
      } else {
        throw `Use npm v${latest}`
      }
    } finally {
      tracker.finish()
    }
  }

  async getLatestNodejsVersion () {
    // XXX get the latest in the current major as well
    const current = process.version
    const currentRange = `^${current}`
    const url = 'https://nodejs.org/dist/index.json'
    const tracker = log.newItem('getLatestNodejsVersion', 1)
    tracker.info('getLatestNodejsVersion', 'Getting Node.js release information')
    try {
      const res = await fetch(url, { method: 'GET', ...this.npm.flatOptions })
      const data = await res.json()
      let maxCurrent = '0.0.0'
      let maxLTS = '0.0.0'
      for (const { lts, version } of data) {
        if (lts && semver.gt(version, maxLTS)) {
          maxLTS = version
        }

        if (semver.satisfies(version, currentRange) && semver.gt(version, maxCurrent)) {
          maxCurrent = version
        }
      }
      const recommended = semver.gt(maxCurrent, maxLTS) ? maxCurrent : maxLTS
      if (semver.gte(process.version, recommended)) {
        return `current: ${current}, recommended: ${recommended}`
      } else {
        throw `Use node ${recommended} (current: ${current})`
      }
    } finally {
      tracker.finish()
    }
  }

  async checkFilesPermission (root, shouldOwn, mask, missingOk) {
    let ok = true

    const tracker = log.newItem(root, 1)

    try {
      const uid = process.getuid()
      const gid = process.getgid()
      const files = new Set([root])
      for (const f of files) {
        tracker.silly('checkFilesPermission', f.slice(root.length + 1))
        const st = await lstat(f).catch(er => {
          // if it can't be missing, or if it can and the error wasn't that it was missing
          if (!missingOk || er.code !== 'ENOENT') {
            ok = false
            tracker.warn('checkFilesPermission', 'error getting info for ' + f)
          }
        })

        tracker.completeWork(1)

        if (!st) {
          continue
        }

        if (shouldOwn && (uid !== st.uid || gid !== st.gid)) {
          tracker.warn('checkFilesPermission', 'should be owner of ' + f)
          ok = false
        }

        if (!st.isDirectory() && !st.isFile()) {
          continue
        }

        try {
          await access(f, mask)
        } catch (er) {
          ok = false
          const msg = `Missing permissions on ${f} (expect: ${maskLabel(mask)})`
          tracker.error('checkFilesPermission', msg)
          continue
        }

        if (st.isDirectory()) {
          const entries = await readdir(f).catch(er => {
            ok = false
            tracker.warn('checkFilesPermission', 'error reading directory ' + f)
            return []
          })
          for (const entry of entries) {
            files.add(resolve(f, entry))
          }
        }
      }
    } finally {
      tracker.finish()
      if (!ok) {
        throw (
          `Check the permissions of files in ${root}` +
          (shouldOwn ? ' (should be owned by current user)' : '')
        )
      } else {
        return ''
      }
    }
  }

  async getGitPath () {
    const tracker = log.newItem('getGitPath', 1)
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
    const tracker = log.newItem('verifyCachedFiles', 1)
    tracker.info('verifyCachedFiles', 'Verifying the npm cache')
    try {
      const stats = await cacache.verify(this.npm.flatOptions.cache)
      const { badContentCount, reclaimedCount, missingContent, reclaimedSize } = stats
      if (badContentCount || reclaimedCount || missingContent) {
        if (badContentCount) {
          tracker.warn('verifyCachedFiles', `Corrupted content removed: ${badContentCount}`)
        }

        if (reclaimedCount) {
          tracker.warn(
            'verifyCachedFiles',
            `Content garbage-collected: ${reclaimedCount} (${reclaimedSize} bytes)`
          )
        }

        if (missingContent) {
          tracker.warn('verifyCachedFiles', `Missing content: ${missingContent}`)
        }

        tracker.warn('verifyCachedFiles', 'Cache issues have been fixed')
      }
      tracker.info(
        'verifyCachedFiles',
        `Verification complete. Stats: ${JSON.stringify(stats, null, 2)}`
      )
      return `verified ${stats.verifiedContent} tarballs`
    } finally {
      tracker.finish()
    }
  }

  async checkNpmRegistry () {
    if (this.npm.flatOptions.registry !== defaultRegistry) {
      throw `Try \`npm config set registry=${defaultRegistry}\``
    } else {
      return `using default registry (${defaultRegistry})`
    }
  }
}

module.exports = Doctor
