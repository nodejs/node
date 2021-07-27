const npm = require('../../lib/npm.js')
const t = require('tap')

const common = require('../common-tap.js')

common.skipIfWindows('this is a unix-only thing')

const errorMessage = require('../../lib/utils/error-message.js')

t.plan(1)

npm.load({ cache: common.cache }, () => {
  npm.config.set('cache', common.cache)
  const er = new Error('access is e, i am afraid')
  er.code = 'EACCES'
  er.errno = -13
  er.path = common.cache + '/src'
  er.dest = common.cache + '/to'

  t.match(errorMessage(er), {
    summary: [
      [
        '',
        new RegExp('\n' +
          'Your cache folder contains root-owned files, due to a bug in\n' +
          'previous versions of npm which has since been addressed.\n' +
          '\n' +
          'To permanently fix this problem, please run:\n' +
          '  sudo chown -R [0-9]+:[0-9]+ ".*npm_cache_cache-eacces-error-message"'
        )
      ]
    ],
    detail: []
  }, 'get the helpful error message')
})
