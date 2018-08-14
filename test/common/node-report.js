const assert = require('assert');
const reportCommon = require('../../deps/node-report/test/common');

exports.findReports = reportCommon.findReports;
exports.validate = (report, options) => {
  t = {
    match: (actual, re, m) => assert.ok(actual.match(re) != null, m),
    plan: () => {},
    test: (name, f) => f(t),
  }
  console.log(t)
  reportCommon.validate(t, report, options); 
}

