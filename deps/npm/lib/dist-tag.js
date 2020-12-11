const log = require('npmlog')
const npa = require('npm-package-arg')
const regFetch = require('npm-registry-fetch')
const semver = require('semver')

const npm = require('./npm.js')
const output = require('./utils/output.js')
const otplease = require('./utils/otplease.js')
const readLocalPkgName = require('./utils/read-local-package.js')
const usageUtil = require('./utils/usage.js')

const usage = usageUtil(
  'dist-tag',
  'npm dist-tag add <pkg>@<version> [<tag>]' +
  '\nnpm dist-tag rm <pkg> <tag>' +
  '\nnpm dist-tag ls [<pkg>]'
)

const completion = function (opts, cb) {
  const argv = opts.conf.argv.remain
  if (argv.length === 2)
    return cb(null, ['add', 'rm', 'ls'])

  switch (argv[2]) {
    default:
      return cb()
  }
}

const cmd = (args, cb) => distTag(args).then(() => cb()).catch(cb)

const distTag = async ([cmdName, pkg, tag]) => {
  const opts = npm.flatOptions
  const has = (items) => new Set(items).has(cmdName)

  if (has(['add', 'a', 'set', 's']))
    return add(pkg, tag, opts)

  if (has(['rm', 'r', 'del', 'd', 'remove']))
    return remove(pkg, tag, opts)

  if (has(['ls', 'l', 'sl', 'list']))
    return list(pkg, opts)

  if (!pkg) {
    // when only using the pkg name the default behavior
    // should be listing the existing tags
    return list(cmdName, opts)
  } else
    throw usage
}

function add (spec, tag, opts) {
  spec = npa(spec || '')
  const version = spec.rawSpec
  const defaultTag = tag || opts.defaultTag

  log.verbose('dist-tag add', defaultTag, 'to', spec.name + '@' + version)

  if (!spec.name || !version || !defaultTag)
    throw usage

  const t = defaultTag.trim()

  if (semver.validRange(t))
    throw new Error('Tag name must not be a valid SemVer range: ' + t)

  return fetchTags(spec, opts).then(tags => {
    if (tags[t] === version) {
      log.warn('dist-tag add', t, 'is already set to version', version)
      return
    }
    tags[t] = version
    const url =
      `/-/package/${spec.escapedName}/dist-tags/${encodeURIComponent(t)}`
    const reqOpts = {
      ...opts,
      method: 'PUT',
      body: JSON.stringify(version),
      headers: {
        'content-type': 'application/json',
      },
      spec,
    }
    return otplease(reqOpts, reqOpts => regFetch(url, reqOpts)).then(() => {
      output(`+${t}: ${spec.name}@${version}`)
    })
  })
}

function remove (spec, tag, opts) {
  spec = npa(spec || '')
  log.verbose('dist-tag del', tag, 'from', spec.name)

  if (!spec.name)
    throw usage

  return fetchTags(spec, opts).then(tags => {
    if (!tags[tag]) {
      log.info('dist-tag del', tag, 'is not a dist-tag on', spec.name)
      throw new Error(tag + ' is not a dist-tag on ' + spec.name)
    }
    const version = tags[tag]
    delete tags[tag]
    const url =
      `/-/package/${spec.escapedName}/dist-tags/${encodeURIComponent(tag)}`
    const reqOpts = {
      ...opts,
      method: 'DELETE',
      spec,
    }
    return otplease(reqOpts, reqOpts => regFetch(url, reqOpts)).then(() => {
      output(`-${tag}: ${spec.name}@${version}`)
    })
  })
}

function list (spec, opts) {
  if (!spec) {
    return readLocalPkgName().then(pkg => {
      if (!pkg)
        throw usage

      return list(pkg, opts)
    })
  }
  spec = npa(spec)

  return fetchTags(spec, opts).then(tags => {
    const msg =
      Object.keys(tags).map(k => `${k}: ${tags[k]}`).sort().join('\n')
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
    {
      ...opts,
      'prefer-online': true,
      spec,
    }
  ).then(data => {
    if (data && typeof data === 'object')
      delete data._etag
    if (!data || !Object.keys(data).length)
      throw new Error('No dist-tags found for ' + spec.name)

    return data
  })
}

module.exports = Object.assign(cmd, { usage, completion })
