const t = require('tap')
const config = require('../../../../lib/utils/config/index.js')
const flatten = require('../../../../lib/utils/config/flatten.js')
const definitions = require('../../../../lib/utils/config/definitions.js')
const describeAll = require('../../../../lib/utils/config/describe-all.js')
t.matchSnapshot(config.shorthands, 'shorthands')

// just spot check a few of these to show that we got defaults assembled
t.match(config.defaults, {
  registry: definitions.registry.default,
  'init-module': definitions['init-module'].default,
})

// is a getter, so changes are reflected
definitions.registry.default = 'https://example.com'
t.strictSame(config.defaults.registry, 'https://example.com')

t.strictSame(config, {
  defaults: config.defaults,
  shorthands: config.shorthands,
  flatten,
  definitions,
  describeAll,
})
