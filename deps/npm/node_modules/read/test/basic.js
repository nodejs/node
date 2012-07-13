var tap = require('tap')
var read = require('../lib/read.js')

if (process.argv[2] === 'child') {
  return child()
}

var spawn = require('child_process').spawn

tap.test('basic', function (t) {
  var child = spawn(process.execPath, [__filename, 'child'])
  var output = ''
  child.stdout.on('data', function (c) {
    console.error('data %s', c)
    output += c
    if (output.match(/Username: \(test-user\) $/)) {
      child.stdin.write('a user\n')
    } else if (output.match(/Password: \(<default hidden>\) $/)) {
      child.stdin.write('a password\n')
    } else if (output.match(/characters: $/)) {
      child.stdin.write('asdf\n')
    } else if (output.match(/Password again: \(<default hidden>\) $/)) {
      child.stdin.write('a password\n')
    }
  })

  var result = ''
  child.stderr.on('data', function (c) {
    result += c
  })

  child.on('close', function () {
    result = JSON.parse(result)
    t.same(result, {"user":"a user","pass":"a password","verify":"a password","four":"asdf","passMatch":true})
    t.equal(output, 'Username: (test-user) Password: (<default hidden>) Enter 4 characters: Password again: (<default hidden>) ')
    t.end()
  })
})

function child () {
  read({prompt: "Username: ", default: "test-user" }, function (er, user) {
    read({prompt: "Password: ", default: "test-pass", silent: true }, function (er, pass) {
      read({prompt: "Enter 4 characters: ", num: 4 }, function (er, four) {
        read({prompt: "Password again: ", default: "test-pass", silent: true }, function (er, pass2) {
          console.error(JSON.stringify({user: user,
                         pass: pass,
                         verify: pass2,
                         four:four,
                         passMatch: (pass === pass2)}))
        })
      })
    })
  })
}
