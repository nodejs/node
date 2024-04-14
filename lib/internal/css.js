'use strict';

const { validateString, validateNumber } = require('internal/validators');
const { ObjectDefineProperty } = primordials;

// Unit factory functions
const numericFactoryFunctions = [
  'Hz', 'Q', 'cap', 'ch', 'cm', 'cqb', 'cqh', 'cqi', 'cqmax', 'cqmin', 'cqw', 'deg', 'dpcm',
  'dpi', 'dppx', 'dvb', 'dvh', 'dvi', 'dvmax', 'dvmin', 'dvw', 'em', 'ex', 'fr', 'grad', 'ic',
  'in', 'kHz', 'lh', 'lvb', 'lvh', 'lvi', 'lvmax', 'lvmin', 'lvw', 'mm', 'ms', 'number', 'pc',
  'percent', 'pt', 'px', 'rad', 'rcap', 'rch', 'rem', 'rex', 'ric', 'rlh', 's', 'svb', 'svh',
  'svi', 'svmax', 'svmin', 'svw', 'turn', 'vb', 'vh', 'vi', 'vmax', 'vmin', 'vw', 'x',
];

const CSS = {};

numericFactoryFunctions.forEach((name) => {
  // Convert function name to lowercase for unit definition
  const unit = name.toLowerCase();
  ObjectDefineProperty(CSS, name, {
    __proto__: null,
    value: (value) => {
      validateNumber(value, 'value');
      return { value, unit };
    },
    enumerable: true,
    writable: false,
    configurable: false,
  });
});

// Define the escape function for CSS
ObjectDefineProperty(CSS, 'escape', {
  __proto__: null,
  value: function(value) {
    validateString(value, 'value');

    let result = '';
    const length = value.length;
    const firstCodeUnit = value.charCodeAt(0);

    // Escape special characters in value
    if (length === 1 && firstCodeUnit === 0x002D) {
      // If the character is '-' (U+002D) and it's the only character
      return '\\' + value;
    }

    // Loop through each character in the value
    for (let index = 0; index < length; index++) {
      const codeUnit = value.charCodeAt(index);

      // Handle special cases for character escaping
      if (codeUnit === 0x0000) {
        result += '\uFFFD'; // Replace NULL (U+0000) with REPLACEMENT CHARACTER (U+FFFD)
      } else if (
        (codeUnit >= 0x0001 && codeUnit <= 0x001F) ||
        codeUnit === 0x007F ||
        (index === 0 && codeUnit >= 0x0030 && codeUnit <= 0x0039) ||
        (index === 1 &&
          codeUnit >= 0x0030 &&
          codeUnit <= 0x0039 &&
          firstCodeUnit === 0x002D)
      ) {
        result += '\\' + codeUnit.toString(16) + ' '; // Escape certain character ranges
      } else if (
        codeUnit >= 0x0080 ||
        codeUnit === 0x002D ||
        codeUnit === 0x005F ||
        (codeUnit >= 0x0030 && codeUnit <= 0x0039) ||
        (codeUnit >= 0x0041 && codeUnit <= 0x005A) ||
        (codeUnit >= 0x0061 && codeUnit <= 0x007A)
      ) {
        result += value.charAt(index); // Keep valid characters unchanged
      } else {
        result += '\\' + value.charAt(index); // Escape invalid characters
      }
    }

    return result; // Return the escaped value
  },
  enumerable: true,
  writable: false,
  configurable: false,
});

module.exports = CSS;

// NOTE: UNSUPPORTED FEATURES
// - CSS.registerProperty
// - CSS.supports
// - CSS.highlights
