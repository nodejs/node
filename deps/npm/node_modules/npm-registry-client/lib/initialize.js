var crypto = require('crypto')
var HttpAgent = require('http').Agent
var HttpsAgent = require('https').Agent

var pkg = require('../package.json')

module.exports = initialize

function initialize (uri, method, accept, headers) {
  if (!this.config.sessionToken) {
    this.config.sessionToken = crypto.randomBytes(8).toString('hex')
    this.log.verbose('request id', this.config.sessionToken)
  }
  if (this.config.isFromCI == null) {
    this.config.isFromCI = Boolean(
      process.env['CI'] === 'true' || process.env['TDDIUM'] ||
      process.env['JENKINS_URL'] || process.env['bamboo.buildKey'] ||
      process.env['GO_PIPELINE_NAME'])
  }

  var opts = {
    url: uri,
    method: method,
    headers: headers,
    localAddress: this.config.proxy.localAddress,
    strictSSL: this.config.ssl.strict,
    cert: this.config.ssl.certificate,
    key: this.config.ssl.key,
    ca: this.config.ssl.ca,
    agent: getAgent.call(this, uri.protocol)
  }

  // allow explicit disabling of proxy in environment via CLI
  //
  // how false gets here is the CLI's problem (it's gross)
  if (this.config.proxy.http === false) {
    opts.proxy = null
  } else {
    // request will not pay attention to the NOPROXY environment variable if a
    // config value named proxy is passed in, even if it's set to null.
    var proxy
    if (uri.protocol === 'https:') {
      proxy = this.config.proxy.https
    } else {
      proxy = this.config.proxy.http
    }
    if (typeof proxy === 'string') opts.proxy = proxy
  }

  headers.version = this.version || pkg.version
  headers.accept = accept

  if (this.refer) headers.referer = this.refer

  headers['npm-session'] = this.config.sessionToken
  headers['npm-in-ci'] = String(this.config.isFromCI)
  headers['user-agent'] = this.config.userAgent
  if (this.config.scope) {
    headers['npm-scope'] = this.config.scope
  }

  return opts
}

function getAgent (protocol) {
  if (protocol === 'https:') {
    if (!this.httpsAgent) {
      this.httpsAgent = new HttpsAgent({
        keepAlive: true,
        maxSockets: this.config.maxSockets,
        localAddress: this.config.proxy.localAddress,
        rejectUnauthorized: this.config.ssl.strict,
        ca: this.config.ssl.ca,
        cert: this.config.ssl.certificate,
        key: this.config.ssl.key
      })
    }

    return this.httpsAgent
  } else {
    if (!this.httpAgent) {
      this.httpAgent = new HttpAgent({
        keepAlive: true,
        maxSockets: this.config.maxSockets,
        localAddress: this.config.proxy.localAddress
      })
    }

    return this.httpAgent
  }
}
