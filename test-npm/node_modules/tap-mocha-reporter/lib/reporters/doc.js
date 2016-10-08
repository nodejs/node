/**
 * Module dependencies.
 */

var Base = require('./base')
  , utils = require('../utils');

/**
 * Expose `Doc`.
 */

exports = module.exports = Doc;

/**
 * Initialize a new `Doc` reporter.
 *
 * @param {Runner} runner
 * @api public
 */

function Doc(runner) {
  Base.call(this, runner);

  var self = this
    , stats = this.stats
    , total = runner.total
    , indents = 2;

  function indent() {
    return Array(indents).join('  ');
  }

  runner.on('suite', function(suite){
    if (suite.root) return;
    ++indents;
    console.log('%s<section class="suite">', indent());
    ++indents;
    console.log('%s<h1>%s</h1>', indent(), utils.escape(suite.title));
    console.log('%s<dl>', indent());
  });

  runner.on('suite end', function(suite){
    if (suite.root) return;
    console.log('%s</dl>', indent());
    --indents;
    console.log('%s</section>', indent());
    --indents;
  });

  runner.on('pass', function(test){
    console.log('%s  <dt>%s</dt>', indent(), utils.escape(test.title));
    var code = utils.escape(utils.clean(test.fn.toString()));
    console.log('%s  <dd><pre><code>%s</code></pre></dd>', indent(), code);
  });

  runner.on('fail', function(test, err){
    console.log('%s  <dt class="error">%s</dt>', indent(), utils.escape(test.title));
    var code = utils.escape(utils.clean(test.fn.toString()));
    console.log('%s  <dd class="error"><pre><code>%s</code></pre></dd>', indent(), code);
    console.log('%s  <dd class="error">%s</dd>', indent(), utils.escape(err));
  });
}
