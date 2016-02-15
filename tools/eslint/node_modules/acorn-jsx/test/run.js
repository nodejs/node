var driver = require("./driver.js");
require("./tests-jsx.js");

function group(name) {
  if (typeof console === "object" && console.group) {
    console.group(name);
  }
}

function groupEnd() {
  if (typeof console === "object" && console.groupEnd) {
    console.groupEnd(name);
  }
}

function log(title, message) {
  if (typeof console === "object") console.log(title, message);
}

var stats, modes = {
  Normal: {
    config: {
      parse: require("..").parse
    }
  }
};

function report(state, code, message) {
  if (state != "ok") {++stats.failed; log(code, message);}
  ++stats.testsRun;
}

group("Errors");

for (var name in modes) {
  group(name);
  var mode = modes[name];
  stats = mode.stats = {testsRun: 0, failed: 0};
  var t0 = +new Date;
  driver.runTests(mode.config, report);
  mode.stats.duration = +new Date - t0;
  groupEnd();
}

groupEnd();

function outputStats(name, stats) {
  log(name + ":", stats.testsRun + " tests run in " + stats.duration + "ms; " +
    (stats.failed ? stats.failed + " failures." : "all passed."));
}

var total = {testsRun: 0, failed: 0, duration: 0};

group("Stats");

for (var name in modes) {
  var stats = modes[name].stats;
  outputStats(name + " parser", stats);
  for (var key in stats) total[key] += stats[key];
}

outputStats("Total", total);

groupEnd();

if (total.failed && typeof process === "object") {
  process.stdout.write("", function() {
    process.exit(1);
  });
}
