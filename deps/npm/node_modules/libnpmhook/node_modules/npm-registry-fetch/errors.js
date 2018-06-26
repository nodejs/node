'use strict'

class HttpErrorBase extends Error {
  constructor (method, res, body, spec) {
    super()
    this.headers = res.headers.raw()
    this.statusCode = res.status
    this.code = `E${res.status}`
    this.method = method
    this.uri = res.url
    this.body = body
  }
}
module.exports.HttpErrorBase = HttpErrorBase

class HttpErrorGeneral extends HttpErrorBase {
  constructor (method, res, body, spec) {
    super(method, res, body, spec)
    this.message = `${res.status} ${res.statusText} - ${
      this.method.toUpperCase()
    } ${
      this.spec || this.uri
    }${
      (body && body.error) ? ' - ' + body.error : ''
    }`
    Error.captureStackTrace(this, HttpErrorGeneral)
  }
}
module.exports.HttpErrorGeneral = HttpErrorGeneral

class HttpErrorAuthOTP extends HttpErrorBase {
  constructor (method, res, body, spec) {
    super(method, res, body, spec)
    this.message = 'OTP required for authentication'
    this.code = 'EOTP'
    Error.captureStackTrace(this, HttpErrorAuthOTP)
  }
}
module.exports.HttpErrorAuthOTP = HttpErrorAuthOTP

class HttpErrorAuthIPAddress extends HttpErrorBase {
  constructor (method, res, body, spec) {
    super(method, res, body, spec)
    this.message = 'Login is not allowed from your IP address'
    this.code = 'EAUTHIP'
    Error.captureStackTrace(this, HttpErrorAuthIPAddress)
  }
}
module.exports.HttpErrorAuthIPAddress = HttpErrorAuthIPAddress

class HttpErrorAuthUnknown extends HttpErrorBase {
  constructor (method, res, body, spec) {
    super(method, res, body, spec)
    this.message = 'Unable to authenticate, need: ' + res.headers.get('www-authenticate')
    Error.captureStackTrace(this, HttpErrorAuthUnknown)
  }
}
module.exports.HttpErrorAuthUnknown = HttpErrorAuthUnknown
