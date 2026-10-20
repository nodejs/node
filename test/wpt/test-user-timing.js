'use strict';

const assert = require('assert');
const { basename } = require('path');
const { WPTRunner } = require('../common/wpt');

const runner = new WPTRunner('user-timing');

runner.pretendGlobalThisAs('Window');

runner.setScriptModifier((script) => {
  if (basename(script.filename) !== 'mark.any.js') return;

  // A scheduling pause between mark() and now() can exceed any fixed
  // tolerance. Check that the mark falls between the surrounding clock
  // readings instead, leaving the upstream fixture unchanged.
  // https://github.com/nodejs/node/issues/40449
  function replace(from, to) {
    assert(script.code.includes(from), `Unexpected contents of ${script.filename}`);
    script.code = script.code.replaceAll(from, to);
  }

  replace('var expectedTimes = new Array();',
          'var beforeTimes = [];\nvar expectedTimes = new Array();');
  replace('self.performance.mark("mark");',
          'beforeTimes.push(self.performance.now());\n    self.performance.mark("mark");');
  replace('assert_approx_equals(entries[index].startTime, expectedTimes[index], testThreshold);',
          'assert_between_inclusive(entries[index].startTime, beforeTimes[index], expectedTimes[index]);');
});

runner.runJsTests();
