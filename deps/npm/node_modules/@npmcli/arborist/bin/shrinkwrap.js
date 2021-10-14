const Shrinkwrap = require('../lib/shrinkwrap.js')
const options = require('./lib/options.js')
require('./lib/logging.js')
require('./lib/timers.js')

const { quiet } = options
Shrinkwrap.load(options)
  .then(s => quiet || console.log(JSON.stringify(s.commit(), 0, 2)))
  .catch(er => {
    console.error('shrinkwrap load failure', er)
    process.exit(1)
  })
