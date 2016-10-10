var common = require('./common.js')
var test = require('tap').test
var reg = 'http://127.0.0.1:15986/'
var path = require('path')

var conf = path.resolve(__dirname, 'fixtures', 'npmrc')
var conf2 = path.resolve(__dirname, 'fixtures', 'npmrc2')
var conf3 = path.resolve(__dirname, 'fixtures', 'npmrc3')

var fs = require('fs')

try { fs.unlinkSync(conf) } catch (er) {}
try { fs.unlinkSync(conf2) } catch (er) {}
try { fs.unlinkSync(conf3) } catch (er) {}

var u = { u: 'user', p: 'pass@:!%\'', e: 'email@example.com' }
var o = { u: 'other', p: 'pass@:!%\'', e: 'other@example.com' }
var t = { u: 'third', p: 'pass@:!%\'', e: '3@example.com' }

test('adduser', fn.bind(null, conf, u))
test('adduser again', fn.bind(null, conf, u))

test('adduser 2', fn.bind(null, conf2, o))
test('adduser 2 again', fn.bind(null, conf2, o))

test('adduser 3', fn.bind(null, conf3, t))
test('adduser 3 again', fn.bind(null, conf3, t))

function fn(conf, u, t) {
  var c = common.npm([
    '--registry=' + reg,
    '--userconf=' + conf,
    'adduser'
  ])
  c.stderr.pipe(process.stderr)
  var buf = ''
  c.stdout.setEncoding('utf8')
  c.stdout.on('data', function(d) {
    buf += d
    if (buf.match(/: /)) {
      console.log(buf)
      switch (buf.split(':')[0]) {
        case 'Username':
          c.stdin.write(u.u + '\r\n')
          break
        case 'Password':
          c.stdin.write(u.p + '\r\n')
          break
        case 'Email':
          c.stdin.end(u.e + '\r\n')
          break
        default:
          throw 'wtf: ' + JSON.stringify(buf)
      }
      buf = ''
    }
  })
  c.on('exit', function(code) {
    t.notOk(code)
    t.end()
  })
}
