// print a banner telling the user to upgrade npm to latest
// but not in CI, and not if we're doing that already.
// Check daily for betas, and weekly otherwise.

const pacote = require('pacote')
const ciDetect = require('@npmcli/ci-detect')
const semver = require('semver')
const chalk = require('chalk')
const { promisify } = require('util')
const stat = promisify(require('fs').stat)
const writeFile = promisify(require('fs').writeFile)
const { resolve } = require('path')

const isGlobalNpmUpdate = npm => {
  return npm.flatOptions.global &&
    ['install', 'update'].includes(npm.command) &&
    npm.argv.some(arg => /^npm(@|$)/.test(arg))
}

// update check frequency
const DAILY = 1000 * 60 * 60 * 24
const WEEKLY = DAILY * 7

// don't put it in the _cacache folder, just in npm's cache
const lastCheckedFile = npm =>
  resolve(npm.flatOptions.cache, '../_update-notifier-last-checked')

const checkTimeout = async (npm, duration) => {
  const t = new Date(Date.now() - duration)
  const f = lastCheckedFile(npm)
  // if we don't have a file, then definitely check it.
  const st = await stat(f).catch(() => ({ mtime: t - 1 }))
  return t > st.mtime
}

const updateNotifier = async (npm, spec = 'latest') => {
  // never check for updates in CI, when updating npm already, or opted out
  if (!npm.config.get('update-notifier') ||
      isGlobalNpmUpdate(npm) ||
      ciDetect()) {
    return null
  }

  // if we're on a prerelease train, then updates are coming fast
  // check for a new one daily.  otherwise, weekly.
  const { version } = npm
  const current = semver.parse(version)

  // if we're on a beta train, always get the next beta
  if (current.prerelease.length) {
    spec = `^${version}`
  }

  // while on a beta train, get updates daily
  const duration = spec !== 'latest' ? DAILY : WEEKLY

  // if we've already checked within the specified duration, don't check again
  if (!(await checkTimeout(npm, duration))) {
    return null
  }

  // if they're currently using a prerelease, nudge to the next prerelease
  // otherwise, nudge to latest.
  const useColor = npm.logColor

  const mani = await pacote.manifest(`npm@${spec}`, {
    // always prefer latest, even if doing --tag=whatever on the cmd
    defaultTag: 'latest',
    ...npm.flatOptions,
  }).catch(() => null)

  // if pacote failed, give up
  if (!mani) {
    return null
  }

  const latest = mani.version

  // if the current version is *greater* than latest, we're on a 'next'
  // and should get the updates from that release train.
  // Note that this isn't another http request over the network, because
  // the packument will be cached by pacote from previous request.
  if (semver.gt(version, latest) && spec === 'latest') {
    return updateNotifier(npm, `^${version}`)
  }

  // if we already have something >= the desired spec, then we're done
  if (semver.gte(version, latest)) {
    return null
  }

  // ok!  notify the user about this update they should get.
  // The message is saved for printing at process exit so it will not get
  // lost in any other messages being printed as part of the command.
  const update = semver.parse(mani.version)
  const type = update.major !== current.major ? 'major'
    : update.minor !== current.minor ? 'minor'
    : update.patch !== current.patch ? 'patch'
    : 'prerelease'
  const typec = !useColor ? type
    : type === 'major' ? chalk.red(type)
    : type === 'minor' ? chalk.yellow(type)
    : chalk.green(type)
  const oldc = !useColor ? current : chalk.red(current)
  const latestc = !useColor ? latest : chalk.green(latest)
  const changelog = `https://github.com/npm/cli/releases/tag/v${latest}`
  const changelogc = !useColor ? `<${changelog}>` : chalk.cyan(changelog)
  const cmd = `npm install -g npm@${latest}`
  const cmdc = !useColor ? `\`${cmd}\`` : chalk.green(cmd)
  const message = `\nNew ${typec} version of npm available! ` +
    `${oldc} -> ${latestc}\n` +
    `Changelog: ${changelogc}\n` +
    `Run ${cmdc} to update!\n`

  return message
}

// only update the notification timeout if we actually finished checking
module.exports = async npm => {
  const notification = await updateNotifier(npm)
  // intentional.  do not await this.  it's a best-effort update.  if this
  // fails, it's ok.  might be using /dev/null as the cache or something weird
  // like that.
  writeFile(lastCheckedFile(npm), '').catch(() => {})
  npm.updateNotification = notification
}
