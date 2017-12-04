'use strict';

var control = require('remark-message-control');

module.exports = lint;

/* `remark-lint`.  This adds support for ignoring stuff from
 * messages (`<!--lint ignore-->`).
 * All rules are in their own packages and presets. */
function lint() {
  this.use(lintMessageControl);
}

function lintMessageControl() {
  return control({name: 'lint', source: 'remark-lint'});
}
