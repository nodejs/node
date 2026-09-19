'use strict';

const assert = require('assert');
const { basename } = require('path');
const { WPTRunner } = require('../common/wpt');

const runner = new WPTRunner('html/webappapis/timers', { concurrency: 1 });

runner.setScriptModifier((script) => {
  if (!['type-long-settimeout.any.js', 'type-long-setinterval.any.js']
    .includes(basename(script.filename))) return;

  // Cancel the failure timer when the test completes so it cannot throw
  // while the runner is still processing the completion message.
  const failureTimer = 'setTimeout(assert_unreached, 100);';
  assert(script.code.includes(failureTimer), `Unexpected contents of ${script.filename}`);
  script.code = script.code.replace(failureTimer,
                                    'const failureTimer = setTimeout(assert_unreached, 100);\n' +
                                    'add_completion_callback(() => clearTimeout(failureTimer));');
});

runner.runJsTests();
