#!/usr/local/bin/node-bench

var Promise = require("events").Promise;

var xml = require("posix").cat("test.xml").wait(),
  path = require("path"),
  sax = require("../lib/sax"),
  saxT = require("../lib/sax-trampoline"),
  
  parser = sax.parser(false, {trim:true}),
  parserT = saxT.parser(false, {trim:true}),
  
  sys = require("sys");


var count = exports.stepsPerLap = 500,
  l = xml.length,
  runs = 0;
exports.countPerLap = 1000;
exports.compare = {
  "switch" : function () {
    // sys.debug("switch runs: "+runs++);
    // for (var x = 0; x < l; x += 1000) {
    //   parser.write(xml.substr(x, 1000))
    // }
    // for (var i = 0; i < count; i ++) {
      parser.write(xml);
      parser.close();
    // }
    // done();
  },
  trampoline : function () {
    // sys.debug("trampoline runs: "+runs++);
    // for (var x = 0; x < l; x += 1000) {
    //   parserT.write(xml.substr(x, 1000))
    // }
    // for (var i = 0; i < count; i ++) {
      parserT.write(xml);
      parserT.close();
    // }
    // done();
  },
};

sys.debug("rock and roll...");