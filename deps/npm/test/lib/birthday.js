const t = require('tap')
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

const Birthday = require('../../lib/birthday.js')
const birthday = new Birthday(npm)

let calledCb = false
birthday.exec([], () => calledCb = true)
t.equal(calledCb, true, 'called the callback')
