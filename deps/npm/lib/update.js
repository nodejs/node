module.exports = update

var url = require('url')
var log = require('npmlog')
var chain = require('slide').chain
var npm = require('./npm.js')
var Installer = require('./install.js').Installer
var usage = require('./utils/usage')

update.usage = usage(
  'update',
  'npm update [-g] [<pkg>...]'
)

update.completion = npm.commands.outdated.completion

function update (args, cb) {
  var dryrun = false
  if (npm.config.get('dry-run')) dryrun = true

  npm.commands.outdated(args, true, function (er, rawOutdated) {
    if (er) return cb(er)
    var outdated = rawOutdated.map(function (ww) {
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

    var wanted = outdated.filter(function (ww) {
      if (ww.current === ww.wanted && ww.wanted !== ww.latest) {
        log.verbose(
          'outdated',
          'not updating', ww.depname,
          "because it's currently at the maximum version that matches its specified semver range"
        )
      }
      return ww.current !== ww.wanted && ww.latest !== 'linked'
    })
    if (wanted.length === 0) return cb()

    log.info('outdated', 'updating', wanted)
    var toInstall = {}
    wanted.forEach(function (ww) {
      // use the initial installation method (repo, tar, git) for updating
      if (url.parse(ww.req).protocol) ww.what = ww.req

      var where = ww.dep.parent && ww.dep.parent.path || ww.dep.path
      if (toInstall[where]) {
        toInstall[where].push(ww.what)
      } else {
        toInstall[where] = [ww.what]
      }
    })
    chain(Object.keys(toInstall).map(function (where) {
      return [new Installer(where, dryrun, toInstall[where]), 'run']
    }), cb)
  })
}
