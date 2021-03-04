const log = require('npmlog')
const npa = require('npm-package-arg')
const regFetch = require('npm-registry-fetch')
const semver = require('semver')

const output = require('./utils/output.js')
const otplease = require('./utils/otplease.js')
const readLocalPkgName = require('./utils/read-local-package.js')
const usageUtil = require('./utils/usage.js')

class DistTag {
  constructor (npm) {
    this.npm = npm
  }

  get usage () {
    return usageUtil(
      'dist-tag',
      'npm dist-tag add <pkg>@<version> [<tag>]' +
      '\nnpm dist-tag rm <pkg> <tag>' +
      '\nnpm dist-tag ls [<pkg>]'
    )
  }

  async completion (opts) {
    const argv = opts.conf.argv.remain
    if (argv.length === 2)
      return ['add', 'rm', 'ls']

    switch (argv[2]) {
      default:
        return []
    }
  }

  exec (args, cb) {
    this.distTag(args).then(() => cb()).catch(cb)
  }

  async distTag ([cmdName, pkg, tag]) {
    const opts = this.npm.flatOptions
    const has = (items) => new Set(items).has(cmdName)

    if (has(['add', 'a', 'set', 's']))
      return this.add(pkg, tag, opts)

    if (has(['rm', 'r', 'del', 'd', 'remove']))
      return this.remove(pkg, tag, opts)

    if (has(['ls', 'l', 'sl', 'list']))
      return this.list(pkg, opts)

    if (!pkg) {
      // when only using the pkg name the default behavior
      // should be listing the existing tags
      return this.list(cmdName, opts)
    } else
      throw this.usage
  }

  async add (spec, tag, opts) {
    spec = npa(spec || '')
    const version = spec.rawSpec
    const defaultTag = tag || opts.defaultTag

    log.verbose('dist-tag add', defaultTag, 'to', spec.name + '@' + version)

    if (!spec.name || !version || !defaultTag)
      throw this.usage

    const t = defaultTag.trim()

    if (semver.validRange(t))
      throw new Error('Tag name must not be a valid SemVer range: ' + t)

    const tags = await this.fetchTags(spec, opts)
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
    await otplease(reqOpts, reqOpts => regFetch(url, reqOpts))
    output(`+${t}: ${spec.name}@${version}`)
  }

  async remove (spec, tag, opts) {
    spec = npa(spec || '')
    log.verbose('dist-tag del', tag, 'from', spec.name)

    if (!spec.name)
      throw this.usage

    const tags = await this.fetchTags(spec, opts)
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
    await otplease(reqOpts, reqOpts => regFetch(url, reqOpts))
    output(`-${tag}: ${spec.name}@${version}`)
  }

  async list (spec, opts) {
    if (!spec) {
      const pkg = await readLocalPkgName(this.npm)
      if (!pkg)
        throw this.usage

      return this.list(pkg, opts)
    }
    spec = npa(spec)

    try {
      const tags = await this.fetchTags(spec, opts)
      const msg =
        Object.keys(tags).map(k => `${k}: ${tags[k]}`).sort().join('\n')
      output(msg)
      return tags
    } catch (err) {
      log.error('dist-tag ls', "Couldn't get dist-tag data for", spec)
      throw err
    }
  }

  async fetchTags (spec, opts) {
    const data = await regFetch.json(
      `/-/package/${spec.escapedName}/dist-tags`,
      { ...opts, 'prefer-online': true, spec }
    )
    if (data && typeof data === 'object')
      delete data._etag
    if (!data || !Object.keys(data).length)
      throw new Error('No dist-tags found for ' + spec.name)

    return data
  }
}
module.exports = DistTag
