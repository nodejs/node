/**
 * Module dependencies.
 */

var Base = require('./base')
  , utils = require('../utils');

/**
 * Constants
 */

var SUITE_PREFIX = '$';

/**
 * Expose `Markdown`.
 */

exports = module.exports = Markdown;

/**
 * Initialize a new `Markdown` reporter.
 *
 * @param {Runner} runner
 * @api public
 */

function Markdown(runner) {
  Base.call(this, runner);

  var self = this
    , stats = this.stats
    , level = 0
    , buf = '';

  function title(str) {
    return Array(level + 1).join('#') + ' ' + str;
  }

  function indent() {
    return Array(level).join('  ');
  }

  function mapTOC(suite, obj) {
    var ret = obj,
        key = SUITE_PREFIX + suite.title;
    obj = obj[key] = obj[key] || { suite: suite };
    suite.suites.forEach(function(suite){
      mapTOC(suite, obj);
    });
    return ret;
  }

  function stringifyTOC(obj, level) {
    ++level;
    var buf = '';
    var link;
    for (var key in obj) {
      if ('suite' == key) continue;
      if (key !== SUITE_PREFIX) {
        link = ' - [' + key.substring(1) + ']';
        link += '(#' + utils.slug(obj[key].suite.fullTitle()) + ')\n';
        buf += Array(level).join('  ') + link;
      }
      buf += stringifyTOC(obj[key], level);
    }
    return buf;
  }

  function generateTOC() {
    return suites.map(generateTOC_).join('')
  }

  function generateTOC_(suite) {
    var obj = mapTOC(suite, {});
    return stringifyTOC(obj, 0);
  }

  var suites = []
  var currentSuite = null
  runner.on('suite', function(suite){
    currentSuite = suite
    if (suite.root) {
      suites.push(suite)
    }
    ++level;
    var slug = utils.slug(suite.fullTitle());
    buf += '<a name="' + slug + '"></a>' + '\n';
    buf += title(suite.title) + '\n';
  });

  runner.on('suite end', function(suite){
    if (suite.ok) {
      buf += '\nok - ' + suite.title + '\n'
    } else {
      buf += '\nnot ok - ' + suite.title + '\n'
    }
    --level;
  });

  runner.on('test', function(test){
    if (!test.ok || test.pending) {
      var code = utils.clean(test.fn.toString());
      buf += test.title + '.\n';
      if (code) {
        buf += '\n```js\n';
        buf += code + '\n';
        buf += '```\n';
      }
      var stack = test.err && test.err.stack
      if (!stack) {
        stack = test.result && test.result.diag && test.result.diag.stack
      }
      if (stack) {
        buf += '\n```\n' + stack + '\n```\n';
      }
      buf += '\n\n';
    }
  });

  runner.on('end', function(){
    process.stdout.write('# TOC\n');
    process.stdout.write(generateTOC());
    process.stdout.write('\n\n');
    process.stdout.write(buf);
  });
}
