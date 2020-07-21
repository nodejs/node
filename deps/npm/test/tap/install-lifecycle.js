var fs = require('graceful-fs')
var path = require('path')
var test = require('tap').test
var common = require('../common-tap.js')
var pkg = common.pkg

test('npm install execution order', function (t) {
  const packageJson = {
    name: 'life-test',
    version: '0.0.1',
    description: 'Test for npm install execution order',
    scripts: {
      install: 'true',
      preinstall: 'true',
      preshrinkwrap: 'true',
      postinstall: 'true',
      postshrinkwrap: 'true',
      shrinkwrap: 'true'
    }
  }
  fs.writeFileSync(path.resolve(pkg, 'package.json'), JSON.stringify(packageJson), 'utf8')
  common.npm(['install', '--loglevel=error'], { cwd: pkg }, function (err, code, stdout, stderr) {
    if (err) throw err

    t.comment(stdout)
    t.comment(stderr)

    const steps = ['preinstall', 'install', 'postinstall', 'preshrinkwrap', 'shrinkwrap', 'postshrinkwrap']
    const expectedLines = steps.map(function (step) {
      return '> ' + packageJson.name + '@' + packageJson.version + ' ' + step
    })
    t.match(stdout, new RegExp(expectedLines.map(common.escapeForRe).join('(.|\n)*')))
    t.end()
  })
})
