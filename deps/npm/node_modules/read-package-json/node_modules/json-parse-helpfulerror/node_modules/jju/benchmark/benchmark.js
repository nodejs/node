#!/usr/bin/env node

var Benchmark = require('benchmark')
var YAML = require('js-yaml')
var JJU = require('../')
var JSON5 = require('json5')
var suite = new Benchmark.Suite

var sample
sample = require('fs').readFileSync(__dirname + '/../package.yaml')
sample = YAML.safeLoad(sample)
sample = JSON.stringify(sample)

var functions = {
  'JSON':    function(x) { JSON.parse(x) },
  'JSON5':   function(x) { JSON5.parse(x) },
  'JJU':     function(x) { JJU.parse(x) },
  'JS-YAML': function(x) { YAML.safeLoad(x) },
}

for (var name in functions) {
  with ({ fn: functions[name] }) {
    suite.add(name, {
      onCycle: function onCycle(event) {
        process.stdout.write('\r\033[2K - ' + event.target)
      },
      fn: function () {
        fn(sample)
      },
    })
  }
}

console.log()
suite.on('cycle', function(event) {
  console.log('\r\033[2K + ' + String(event.target))
})
.run()

process.on('exit', function() { console.log() })
