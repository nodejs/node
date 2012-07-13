// test that we can tunnel a https request over an http proxy
// keeping all the CA and whatnot intact.
//
// Note: this requires that squid is installed.
// If the proxy fails to start, we'll just log a warning and assume success.

var server = require('./server')
  , assert = require('assert')
  , request = require('../main.js')
  , fs = require('fs')
  , path = require('path')
  , caFile = path.resolve(__dirname, 'ssl/npm-ca.crt')
  , ca = fs.readFileSync(caFile)
  , child_process = require('child_process')
  , sqConf = path.resolve(__dirname, 'squid.conf')
  , sqArgs = ['-f', sqConf, '-N', '-d', '5']
  , proxy = 'http://localhost:3128'
  , hadError = null

var squid = child_process.spawn('squid', sqArgs);
var ready = false

squid.stderr.on('data', function (c) {
  console.error('SQUIDERR ' + c.toString().trim().split('\n')
               .join('\nSQUIDERR '))
  ready = c.toString().match(/ready to serve requests/i)
})

squid.stdout.on('data', function (c) {
  console.error('SQUIDOUT ' + c.toString().trim().split('\n')
               .join('\nSQUIDOUT '))
})

squid.on('exit', function (c) {
  console.error('exit '+c)
  if (c && !ready) {
    console.error('squid must be installed to run this test.')
    c = null
    hadError = null
    process.exit(0)
    return
  }

  if (c) {
    hadError = hadError || new Error('Squid exited with '+c)
  }
  if (hadError) throw hadError
})

setTimeout(function F () {
  if (!ready) return setTimeout(F, 100)
  request({ uri: 'https://registry.npmjs.org/request/'
          , proxy: 'http://localhost:3128'
          , ca: ca
          , json: true }, function (er, body) {
    hadError = er
    console.log(er || typeof body)
    if (!er) console.log("ok")
    squid.kill('SIGKILL')
  })
}, 100)
