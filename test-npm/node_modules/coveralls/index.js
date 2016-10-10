var minimist = require('minimist');

// this needs to go before the other require()s so that
// the other files can already use index.options
exports.options = minimist(process.argv.slice(2), {
  boolean: ['verbose', 'stdout'],
  alias: { 'v': 'verbose', 's': 'stdout' }
});

var dir = './lib/';
exports.convertLcovToCoveralls = require(dir + 'convertLcovToCoveralls');
exports.sendToCoveralls = require(dir + 'sendToCoveralls');
exports.getBaseOptions = require(dir + 'getOptions').getBaseOptions;
exports.getOptions = require(dir + 'getOptions').getOptions;
exports.handleInput = require(dir + 'handleInput');
exports.logger = require(dir + 'logger');
