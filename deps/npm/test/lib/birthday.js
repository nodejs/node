const t = require('tap')
const requireInject = require('require-inject')
const npm = {
  flatOptions: {
    yes: false,
    package: [],
  },
  commands: {
    exec: (args, cb) => {
      t.equal(npm.flatOptions.yes, true, 'should say yes')
      t.strictSame(npm.flatOptions.package, ['@npmcli/npm-birthday'],
        'uses correct package')
      t.strictSame(args, ['npm-birthday'], 'called with correct args')
      t.match(cb, Function, 'callback is a function')
      cb()
    },
  },
}

const birthday = requireInject('../../lib/birthday.js', {
  '../../lib/npm.js': npm,
})

let calledCb = false
birthday([], () => calledCb = true)
t.equal(calledCb, true, 'called the callback')
