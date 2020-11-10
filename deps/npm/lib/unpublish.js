/* eslint-disable standard/no-callback-literal */
'use strict'

module.exports = unpublish

const BB = require('bluebird')

const figgyPudding = require('figgy-pudding')
const libaccess = require('libnpm/access')
const libunpub = require('libnpm/unpublish')
const log = require('npmlog')
const npa = require('npm-package-arg')
const npm = require('./npm.js')
const npmConfig = require('./config/figgy-config.js')
const npmFetch = require('npm-registry-fetch')
const otplease = require('./utils/otplease.js')
const output = require('./utils/output.js')
const path = require('path')
const readJson = BB.promisify(require('read-package-json'))
const usage = require('./utils/usage.js')
const whoami = BB.promisify(require('./whoami.js'))

unpublish.usage = usage(
  'unpublish',
  '\nnpm unpublish [<@scope>/]<pkg>@<version>' +
    '\nnpm unpublish [<@scope>/]<pkg> --force'
)

function UsageError () {
  throw Object.assign(new Error(`Usage: ${unpublish.usage}`), {
    code: 'EUSAGE'
  })
}

const UnpublishConfig = figgyPudding({
  force: {},
  loglevel: {},
  silent: {}
})

unpublish.completion = function (cliOpts, cb) {
  if (cliOpts.conf.argv.remain.length >= 3) return cb()

  whoami([], true).then(username => {
    if (!username) { return [] }
    const opts = UnpublishConfig(npmConfig())
    return libaccess.lsPackages(username, opts).then(access => {
      // do a bit of filtering at this point, so that we don't need
      // to fetch versions for more than one thing, but also don't
      // accidentally a whole project.
      let pkgs = Object.keys(access)
      if (!cliOpts.partialWord || !pkgs.length) { return pkgs }
      const pp = npa(cliOpts.partialWord).name
      pkgs = pkgs.filter(p => !p.indexOf(pp))
      if (pkgs.length > 1) return pkgs
      return npmFetch.json(npa(pkgs[0]).escapedName, opts).then(doc => {
        const vers = Object.keys(doc.versions)
        if (!vers.length) {
          return pkgs
        } else {
          return vers.map(v => `${pkgs[0]}@${v}`)
        }
      })
    })
  }).nodeify(cb)
}

function unpublish (args, cb) {
  if (args.length > 1) return cb(unpublish.usage)

  const spec = args.length && npa(args[0])
  const opts = UnpublishConfig(npmConfig())
  const version = spec.rawSpec
  BB.try(() => {
    log.silly('unpublish', 'args[0]', args[0])
    log.silly('unpublish', 'spec', spec)
    if (!version && !opts.force) {
      throw Object.assign(new Error(
        'Refusing to delete entire project.\n' +
        'Run with --force to do this.\n' +
        unpublish.usage
      ), { code: 'EUSAGE' })
    }
    if (!spec || path.resolve(spec.name) === npm.localPrefix) {
      // if there's a package.json in the current folder, then
      // read the package name and version out of that.
      const cwdJson = path.join(npm.localPrefix, 'package.json')
      return readJson(cwdJson).then(data => {
        log.verbose('unpublish', data)
        return otplease(opts, opts => {
          return libunpub(npa.resolve(data.name, data.version), opts.concat(data.publishConfig))
        })
      }, err => {
        if (err && err.code !== 'ENOENT' && err.code !== 'ENOTDIR') {
          throw err
        } else {
          UsageError()
        }
      })
    } else {
      return otplease(opts, opts => libunpub(spec, opts))
    }
  }).then(
    ret => {
      if (!opts.silent && opts.loglevel !== 'silent') {
        output(`- ${spec.name}${
          spec.type === 'version' ? `@${spec.rawSpec}` : ''
        }`)
      }
      cb(null, ret)
    },
    err => err.code === 'EUSAGE' ? cb(err.message) : cb(err)
  )
}
