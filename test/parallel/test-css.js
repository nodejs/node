'use strict';

require('../common');
const assert = require('node:assert');

const numericFactoryFunctions = [
  'Hz', 'Q', 'cap', 'ch', 'cm', 'cqb', 'cqh', 'cqi', 'cqmax', 'cqmin', 'cqw', 'deg', 'dpcm',
  'dpi', 'dppx', 'dvb', 'dvh', 'dvi', 'dvmax', 'dvmin', 'dvw', 'em', 'ex', 'fr', 'grad', 'ic',
  'in', 'kHz', 'lh', 'lvb', 'lvh', 'lvi', 'lvmax', 'lvmin', 'lvw', 'mm', 'ms', 'number', 'pc',
  'percent', 'pt', 'px', 'rad', 'rcap', 'rch', 'rem', 'rex', 'ric', 'rlh', 's', 'svb', 'svh',
  'svi', 'svmax', 'svmin', 'svw', 'turn', 'vb', 'vh', 'vi', 'vmax', 'vmin', 'vw', 'x',
];

for (const unit of numericFactoryFunctions) {
  const value = 10; // Change the value for testing
  const result = CSS[unit](value);
  assert.deepStrictEqual(result, { value, unit: unit.toLowerCase() });

  assert.throws(() => CSS[unit]('invalid'), {
    message: 'The "value" argument must be of type number. Received type string (\'invalid\')'
  });
}

// Testing CSS.escape
assert.strictEqual(CSS.escape('\\n'), '\\\\n');
assert.strictEqual(CSS.escape('\u0000'), '\uFFFD');
