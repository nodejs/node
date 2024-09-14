'use strict';

const common = require('../../common.js')

const bench = common.createBenchmark(main, {
  n: [5e6],
  addon: ['binding', 'binding_node_api_v8'],
  implem: ['createExternalBuffer'],
})

async function main({ n, implem, addon }) {
  const bindingPath = `./build/${buildType}/${addon}`
  const binding = await import(bindingPath)
  binding[implem](bench, n)
}
