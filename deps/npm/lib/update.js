'use strict'
module.exports = update

const url = require('url')
const log = require('npmlog')
const Bluebird = require('bluebird')
const npm = require('./npm.js')
const Installer = require('./install.js').Installer
const usage = require('./utils/usage')
const outdated = Bluebird.promisify(npm.commands.outdated)

update.usage = usage(
  'update',
  'npm update [-g] [<pkg>...]'
)

update.completion = npm.commands.outdated.completion

function update (args, cb) {
  return update_(args).asCallback(cb)
}

function update_ (args) {
  let dryrun = false
  if (npm.config.get('dry-run')) dryrun = true

  log.verbose('update', 'computing outdated modules to update')
  return outdated(args, true).then((rawOutdated) => {
    const outdated = rawOutdated.map(function (ww) {
      return {
        dep: ww[0],
        depname: ww[1],
        current: ww[2],
        wanted: ww[3],
        latest: ww[4],
        req: ww[5],
        what: ww[1] + '@' + ww[3]
      }
    })

    const wanted = outdated.filter(function (ww) {
      if (ww.current === ww.wanted && ww.wanted !== ww.latest) {
        log.verbose(
          'outdated',
          'not updating', ww.depname,
          "because it's currently at the maximum version that matches its specified semver range"
        )
      }
      return ww.current !== ww.wanted && ww.latest !== 'linked'
    })
    if (wanted.length === 0) return

    log.info('outdated', 'updating', wanted)
    const toInstall = {}

    wanted.forEach(function (ww) {
      // use the initial installation method (repo, tar, git) for updating
      if (url.parse(ww.req).protocol) ww.what = ww.req

      const where = ww.dep.parent && ww.dep.parent.path || ww.dep.path
      const isTransitive = !(ww.dep.requiredBy || []).some((p) => p.isTop)
      const key = where + ':' + String(isTransitive)
      if (!toInstall[key]) toInstall[key] = {where: where, opts: {saveOnlyLock: isTransitive}, what: []}
      if (toInstall[key].what.indexOf(ww.what) === -1) toInstall[key].what.push(ww.what)
    })
    return Bluebird.each(Object.keys(toInstall), (key) => {
      const deps = toInstall[key]
      const inst = new Installer(deps.where, dryrun, deps.what, deps.opts)
      return inst.run()
    })
  })
}
