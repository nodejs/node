var fs = require('graceful-fs')
var path = require('path')
var test = require('tap').test
var common = require('../common-tap.js')
var pkg = common.pkg

test('npm shrinkwrap execution order', function (t) {
  fs.writeFileSync(path.resolve(pkg, 'package.json'), JSON.stringify({
    author: 'Simen Bekkhus',
    name: 'shrinkwrap-lifecycle',
    shrinkwrap: '0.0.0',
    description: 'Test for npm shrinkwrap execution order',
    scripts: {
      preshrinkwrap: 'echo this happens first',
      shrinkwrap: 'echo this happens second',
      postshrinkwrap: 'echo this happens third'
    }
  }), 'utf8')
  common.npm(['shrinkwrap', '--loglevel=error'], { cwd: pkg }, function (err, code, stdout, stderr) {
    if (err) throw err

    t.comment(stdout)
    t.comment(stderr)
    var indexOfFirst = stdout.indexOf('echo this happens first')
    var indexOfSecond = stdout.indexOf('echo this happens second')
    var indexOfThird = stdout.indexOf('echo this happens third')

    t.ok(indexOfFirst >= 0)
    t.ok(indexOfSecond >= 0)
    t.ok(indexOfThird >= 0)

    t.ok(indexOfFirst < indexOfSecond)
    t.ok(indexOfSecond < indexOfThird)

    t.end()
  })
})
