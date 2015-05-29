'use strict'
var gitHosts = require('./git-host-info.js')

var GitHost = module.exports = function (type, user, auth, project, committish, defaultRepresentation) {
  var gitHostInfo = this
  gitHostInfo.type = type
  Object.keys(gitHosts[type]).forEach(function (key) {
    gitHostInfo[key] = gitHosts[type][key]
  })
  gitHostInfo.user = user
  gitHostInfo.auth = auth
  gitHostInfo.project = project
  gitHostInfo.committish = committish
  gitHostInfo.default = defaultRepresentation
}
GitHost.prototype = {}

GitHost.prototype.hash = function () {
  return this.committish ? '#' + this.committish : ''
}

GitHost.prototype._fill = function (template, vars) {
  if (!template) return
  if (!vars) vars = {}
  var self = this
  Object.keys(this).forEach(function (key) {
    if (self[key] != null && vars[key] == null) vars[key] = self[key]
  })
  var rawAuth = vars.auth
  var rawComittish = vars.committish
  Object.keys(vars).forEach(function (key) {
    vars[key] = encodeURIComponent(vars[key])
  })
  vars['auth@'] = rawAuth ? rawAuth + '@' : ''
  vars['#committish'] = rawComittish ? '#' + rawComittish : ''
  vars['/tree/committish'] = vars.committish
                          ? '/' + vars.treepath + '/' + vars.committish
                          : ''
  vars['/committish'] = vars.committish ? '/' + vars.committish : ''
  vars.committish = vars.committish || 'master'
  var res = template
  Object.keys(vars).forEach(function (key) {
    res = res.replace(new RegExp('[{]' + key + '[}]', 'g'), vars[key])
  })
  return res
}

GitHost.prototype.ssh = function () {
  return this._fill(this.sshtemplate)
}

GitHost.prototype.sshurl = function () {
  return this._fill(this.sshurltemplate)
}

GitHost.prototype.browse = function () {
  return this._fill(this.browsetemplate)
}

GitHost.prototype.docs = function () {
  return this._fill(this.docstemplate)
}

GitHost.prototype.bugs = function () {
  return this._fill(this.bugstemplate)
}

GitHost.prototype.https = function () {
  return this._fill(this.httpstemplate)
}

GitHost.prototype.git = function () {
  return this._fill(this.gittemplate)
}

GitHost.prototype.shortcut = function () {
  return this._fill(this.shortcuttemplate)
}

GitHost.prototype.path = function () {
  return this._fill(this.pathtemplate)
}

GitHost.prototype.file = function (P) {
  return this._fill(this.filetemplate, {
    path: P.replace(/^[/]+/g, '')
  })
}

GitHost.prototype.getDefaultRepresentation = function () {
  return this.default
}

GitHost.prototype.toString = function () {
  return (this[this.default] || this.sshurl).call(this)
}
