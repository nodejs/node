#!/usr/bin/env node

/* eslint-disable no-var */
/* eslint-disable flowtype/require-valid-file-annotation */
'use strict';

var ver = process.versions.node;
var majorVer = parseInt(ver.split('.')[0], 10);

if (majorVer < 4) {
  console.error('Node version ' + ver + ' is not supported, please use Node.js 4.0 or higher.');
  process.exit(1); // eslint-disable-line no-process-exit
} else {
  try {
    require(__dirname + '/../lib/v8-compile-cache.js');
  } catch (err) {
    // We don't have/need this on legacy builds and dev builds
  }

  // Just requiring this package will trigger a yarn run since the
  // `require.main === module` check inside `cli/index.js` will always
  // be truthy when built with webpack :(
  // `lib/cli` may be `lib/cli/index.js` or `lib/cli.js` depending on the build.
  var cli = require(__dirname + '/../lib/cli');
  if (!cli.autoRun) {
    cli.default().catch(function(error) {
      console.error(error.stack || error.message || error);
      process.exitCode = 1;
    });
  }
}
