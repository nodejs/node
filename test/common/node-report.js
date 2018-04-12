const assert = require('assert');
const reportCommon = require('../../../deps/node-report/test/common');

exports.findReports = reportCommon.findReports;
exports.validate = (report, options) => {
  t = {
    test: (name, f) => f(),
  }
  reportCommon.validate(t, report, options); 
}

