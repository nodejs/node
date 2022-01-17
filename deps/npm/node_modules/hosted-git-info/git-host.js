'use strict'
const gitHosts = require('./git-host-info.js')

class GitHost {
  constructor (type, user, auth, project, committish, defaultRepresentation, opts = {}) {
    Object.assign(this, gitHosts[type])
    this.type = type
    this.user = user
    this.auth = auth
    this.project = project
    this.committish = committish
    this.default = defaultRepresentation
    this.opts = opts
  }

  hash () {
    return this.committish ? `#${this.committish}` : ''
  }

  ssh (opts) {
    return this._fill(this.sshtemplate, opts)
  }

  _fill (template, opts) {
    if (typeof template === 'function') {
      const options = { ...this, ...this.opts, ...opts }

      // the path should always be set so we don't end up with 'undefined' in urls
      if (!options.path) {
        options.path = ''
      }

      // template functions will insert the leading slash themselves
      if (options.path.startsWith('/')) {
        options.path = options.path.slice(1)
      }

      if (options.noCommittish) {
        options.committish = null
      }

      const result = template(options)
      return options.noGitPlus && result.startsWith('git+') ? result.slice(4) : result
    }

    return null
  }

  sshurl (opts) {
    return this._fill(this.sshurltemplate, opts)
  }

  browse (path, fragment, opts) {
    // not a string, treat path as opts
    if (typeof path !== 'string') {
      return this._fill(this.browsetemplate, path)
    }

    if (typeof fragment !== 'string') {
      opts = fragment
      fragment = null
    }
    return this._fill(this.browsefiletemplate, { ...opts, fragment, path })
  }

  docs (opts) {
    return this._fill(this.docstemplate, opts)
  }

  bugs (opts) {
    return this._fill(this.bugstemplate, opts)
  }

  https (opts) {
    return this._fill(this.httpstemplate, opts)
  }

  git (opts) {
    return this._fill(this.gittemplate, opts)
  }

  shortcut (opts) {
    return this._fill(this.shortcuttemplate, opts)
  }

  path (opts) {
    return this._fill(this.pathtemplate, opts)
  }

  tarball (opts) {
    return this._fill(this.tarballtemplate, { ...opts, noCommittish: false })
  }

  file (path, opts) {
    return this._fill(this.filetemplate, { ...opts, path })
  }

  getDefaultRepresentation () {
    return this.default
  }

  toString (opts) {
    if (this.default && typeof this[this.default] === 'function') {
      return this[this.default](opts)
    }

    return this.sshurl(opts)
  }
}
module.exports = GitHost
