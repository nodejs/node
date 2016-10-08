#!/usr/bin/env node
if (process.env.xyz) {
  console.log('in t.js, xyz=%j', process.env.xyz)
  console.log('gc is a', typeof gc)
  console.log('%j', process.argv)
  console.log('about to run the main file\u001b[32m')
  require('./index.js').runMain()
  console.log('\u001b[31mran wrapped main')
  return
}

var wrap = require('./index.js')

var unwrap = wrap(['--expose_gc', __filename, ' a $ b '], { xyz: 'ABC' })

console.log('about to run child process')
console.log('gc is a', typeof gc)
var cp = require('child_process')
var child = cp.exec(process.execPath + ' $(which tap ) -h', { env: { foo: 'asdf', PATH:process.env.PATH } }, function (er, out, err) {
  console.error('returned')
  console.error('error = ', er)
  console.error('outlen=', out.length)
  console.error('\u001b[31m' + out + '\u001b[m')
  console.error('errlen=', err.length)
  process.stderr.write(err)
})
