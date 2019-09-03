/* eslint-disable standard/no-callback-literal */
module.exports = distTag

const BB = require('bluebird')

const figgyPudding = require('figgy-pudding')
const log = require('npmlog')
const npa = require('libnpm/parse-arg')
const npmConfig = require('./config/figgy-config.js')
const output = require('./utils/output.js')
const otplease = require('./utils/otplease.js')
const readLocalPkg = BB.promisify(require('./utils/read-local-package.js'))
const regFetch = require('libnpm/fetch')
const semver = require('semver')
const usage = require('./utils/usage')

const DistTagOpts = figgyPudding({
  tag: {}
})

distTag.usage = usage(
  'dist-tag',
  'npm dist-tag add <pkg>@<version> [<tag>]' +
  '\nnpm dist-tag rm <pkg> <tag>' +
  '\nnpm dist-tag ls [<pkg>]'
)

distTag.completion = function (opts, cb) {
  var argv = opts.conf.argv.remain
  if (argv.length === 2) {
    return cb(null, ['add', 'rm', 'ls'])
  }

  switch (argv[2]) {
    default:
      return cb()
  }
}

function UsageError () {
  throw Object.assign(new Error('Usage:\n' + distTag.usage), {
    code: 'EUSAGE'
  })
}

function distTag ([cmd, pkg, tag], cb) {
  const opts = DistTagOpts(npmConfig())
  return BB.try(() => {
    switch (cmd) {
      case 'add': case 'a': case 'set': case 's':
        return add(pkg, tag, opts)
      case 'rm': case 'r': case 'del': case 'd': case 'remove':
        return remove(pkg, tag, opts)
      case 'ls': case 'l': case 'sl': case 'list':
        return list(pkg, opts)
      default:
        if (!pkg) {
          return list(cmd, opts)
        } else {
          UsageError()
        }
    }
  }).then(
    x => cb(null, x),
    err => {
      if (err.code === 'EUSAGE') {
        cb(err.message)
      } else {
        cb(err)
      }
    }
  )
}

function add (spec, tag, opts) {
  spec = npa(spec || '')
  const version = spec.rawSpec
  const t = (tag || opts.tag).trim()

  log.verbose('dist-tag add', t, 'to', spec.name + '@' + version)

  if (!spec || !version || !t) UsageError()

  if (semver.validRange(t)) {
    throw new Error('Tag name must not be a valid SemVer range: ' + t)
  }

  return fetchTags(spec, opts).then(tags => {
    if (tags[t] === version) {
      log.warn('dist-tag add', t, 'is already set to version', version)
      return
    }
    tags[t] = version
    const url = `/-/package/${spec.escapedName}/dist-tags/${encodeURIComponent(t)}`
    const reqOpts = opts.concat({
      method: 'PUT',
      body: JSON.stringify(version),
      headers: {
        'content-type': 'application/json'
      },
      spec
    })
    return otplease(reqOpts, reqOpts => regFetch(url, reqOpts)).then(() => {
      output(`+${t}: ${spec.name}@${version}`)
    })
  })
}

function remove (spec, tag, opts) {
  spec = npa(spec || '')
  log.verbose('dist-tag del', tag, 'from', spec.name)

  return fetchTags(spec, opts).then(tags => {
    if (!tags[tag]) {
      log.info('dist-tag del', tag, 'is not a dist-tag on', spec.name)
      throw new Error(tag + ' is not a dist-tag on ' + spec.name)
    }
    const version = tags[tag]
    delete tags[tag]
    const url = `/-/package/${spec.escapedName}/dist-tags/${encodeURIComponent(tag)}`
    const reqOpts = opts.concat({
      method: 'DELETE',
      spec
    })
    return otplease(reqOpts, reqOpts => regFetch(url, reqOpts)).then(() => {
      output(`-${tag}: ${spec.name}@${version}`)
    })
  })
}

function list (spec, opts) {
  if (!spec) {
    return readLocalPkg().then(pkg => {
      if (!pkg) { UsageError() }
      return list(pkg, opts)
    })
  }
  spec = npa(spec)

  return fetchTags(spec, opts).then(tags => {
    var msg = Object.keys(tags).map(k => `${k}: ${tags[k]}`).sort().join('\n')
    output(msg)
    return tags
  }, err => {
    log.error('dist-tag ls', "Couldn't get dist-tag data for", spec)
    throw err
  })
}

function fetchTags (spec, opts) {
  return regFetch.json(
    `/-/package/${spec.escapedName}/dist-tags`,
    opts.concat({
      'prefer-online': true,
      spec
    })
  ).then(data => {
    if (data && typeof data === 'object') delete data._etag
    if (!data || !Object.keys(data).length) {
      throw new Error('No dist-tags found for ' + spec.name)
    }
    return data
  })
}
