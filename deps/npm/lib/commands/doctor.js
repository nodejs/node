const cacache = require('cacache')
const fs = require('fs')
const fetch = require('make-fetch-happen')
const Table = require('cli-table3')
const which = require('which')
const pacote = require('pacote')
const { resolve } = require('path')
const semver = require('semver')
const { promisify } = require('util')
const log = require('../utils/log-shim.js')
const ping = require('../utils/ping.js')
const { defaults } = require('@npmcli/config/lib/definitions')
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

const subcommands = [
  {
    groups: ['ping', 'registry'],
    title: 'npm ping',
    cmd: 'checkPing',
  }, {
    groups: ['versions'],
    title: 'npm -v',
    cmd: 'getLatestNpmVersion',
  }, {
    groups: ['versions'],
    title: 'node -v',
    cmd: 'getLatestNodejsVersion',
  }, {
    groups: ['registry'],
    title: 'npm config get registry',
    cmd: 'checkNpmRegistry',
  }, {
    groups: ['environment'],
    title: 'git executable in PATH',
    cmd: 'getGitPath',
  }, {
    groups: ['environment'],
    title: 'global bin folder in PATH',
    cmd: 'getBinPath',
  }, {
    groups: ['permissions', 'cache'],
    title: 'Perms check on cached files',
    cmd: 'checkCachePermission',
    windows: false,
  }, {
    groups: ['permissions'],
    title: 'Perms check on local node_modules',
    cmd: 'checkLocalModulesPermission',
    windows: false,
  }, {
    groups: ['permissions'],
    title: 'Perms check on global node_modules',
    cmd: 'checkGlobalModulesPermission',
    windows: false,
  }, {
    groups: ['permissions'],
    title: 'Perms check on local bin folder',
    cmd: 'checkLocalBinPermission',
    windows: false,
  }, {
    groups: ['permissions'],
    title: 'Perms check on global bin folder',
    cmd: 'checkGlobalBinPermission',
    windows: false,
  }, {
    groups: ['cache'],
    title: 'Verify cache contents',
    cmd: 'verifyCachedFiles',
    windows: false,
  },
  // TODO:
  // group === 'dependencies'?
  //   - ensure arborist.loadActual() runs without errors and no invalid edges
  //   - ensure package-lock.json matches loadActual()
  //   - verify loadActual without hidden lock file matches hidden lockfile
  // group === '???'
  //   - verify all local packages have bins linked
  // What is the fix for these?
]
const BaseCommand = require('../base-command.js')
class Doctor extends BaseCommand {
  static description = 'Check your npm environment'
  static name = 'doctor'
  static params = ['registry']
  static ignoreImplicitWorkspace = false
  static usage = [`[${subcommands.flatMap(s => s.groups)
    .filter((value, index, self) => self.indexOf(value) === index)
    .join('] [')}]`]

  static subcommands = subcommands

  // minimum width of check column, enough for the word `Check`
  #checkWidth = 5

  async exec (args) {
    log.info('Running checkup')
    let allOk = true

    const actions = this.actions(args)
    this.#checkWidth = actions.reduce((length, item) =>
      Math.max(item.title.length, length), this.#checkWidth)

    if (!this.npm.silent) {
      this.output(['Check', 'Value', 'Recommendation/Notes'].map(h => this.npm.chalk.underline(h)))
    }
    // Do the actual work
    for (const { title, cmd } of actions) {
      const item = [title]
      try {
        item.push(true, await this[cmd]())
      } catch (err) {
        item.push(false, err)
      }
      if (!item[1]) {
        allOk = false
        item[0] = this.npm.chalk.red(item[0])
        item[1] = this.npm.chalk.red('not ok')
        item[2] = this.npm.chalk.magenta(String(item[2]))
      } else {
        item[1] = this.npm.chalk.green('ok')
      }
      if (!this.npm.silent) {
        this.output(item)
      }
    }

    if (!allOk) {
      if (this.npm.silent) {
        /* eslint-disable-next-line max-len */
        throw new Error('Some problems found. Check logs or disable silent mode for recommendations.')
      } else {
        throw new Error('Some problems found. See above for recommendations.')
      }
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

  async getBinPath (dir) {
    const tracker = log.newItem('getBinPath', 1)
    tracker.info('getBinPath', 'Finding npm global bin in your PATH')
    if (!process.env.PATH.includes(this.npm.globalBin)) {
      throw new Error(`Add ${this.npm.globalBin} to your $PATH`)
    }
    return this.npm.globalBin
  }

  async checkCachePermission () {
    return this.checkFilesPermission(this.npm.cache, true, R_OK)
  }

  async checkLocalModulesPermission () {
    return this.checkFilesPermission(this.npm.localDir, true, R_OK | W_OK, true)
  }

  async checkGlobalModulesPermission () {
    return this.checkFilesPermission(this.npm.globalDir, false, R_OK)
  }

  async checkLocalBinPermission () {
    return this.checkFilesPermission(this.npm.localBin, false, R_OK | W_OK | X_OK, true)
  }

  async checkGlobalBinPermission () {
    return this.checkFilesPermission(this.npm.globalBin, false, X_OK)
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
        throw new Error("Install git and ensure it's in your PATH.")
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
    if (this.npm.flatOptions.registry !== defaults.registry) {
      throw `Try \`npm config set registry=${defaults.registry}\``
    } else {
      return `using default registry (${defaults.registry})`
    }
  }

  output (row) {
    const t = new Table({
      chars: {
        top: '',
        'top-mid': '',
        'top-left': '',
        'top-right': '',
        bottom: '',
        'bottom-mid': '',
        'bottom-left': '',
        'bottom-right': '',
        left: '',
        'left-mid': '',
        mid: '',
        'mid-mid': '',
        right: '',
        'right-mid': '',
        middle: '  ',
      },
      style: {
        'padding-left': 0,
        'padding-right': 0,
        // setting border here is not necessary visually since we've already
        // zeroed out all the chars above, but without it cli-table3 will wrap
        // some of the separator spaces with ansi codes which show up in
        // snapshots.
        border: 0,
      },
      colWidths: [this.#checkWidth, 6],
    })
    t.push(row)
    this.npm.output(t.toString())
  }

  actions (params) {
    return this.constructor.subcommands.filter(subcmd => {
      if (process.platform === 'win32' && subcmd.windows === false) {
        return false
      }
      if (params.length) {
        return params.some(param => subcmd.groups.includes(param))
      }
      return true
    })
  }
}

module.exports = Doctor
