'use strict';
require('../common');
const { Console } = require('console');

// Work with no color
process.env.NODE_DISABLE_COLORS = true;
process.env.FORCE_COLOR = 0;

const options = {
  stdout: process.stdout,
  stderr: process.stderr,
  ignoreErrors: true,
  highlight: {
    // Indicators should always work even if no coloring enabled
    warn: { bgColor: 'bgYellow', indicator: '[W]' },
    error: { bgColor: 'bgRed', indicator: '[E]' },
  }
};

const newInstance = new Console(options);
newInstance.warn(new Error('test'));
newInstance.error(new Error('test'));
