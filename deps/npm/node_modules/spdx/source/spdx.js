// spdx.js
// =======
// SPDX License Expression Syntax parser

// Validation
// ----------

// Require the generated parser.
var parser = require('./parser.generated.js').parser;

exports.parse = function(argument) {
  return parser.parse(argument);
};

var containsRepeatedSpace = /\s{2,}/;

exports.valid = function(argument) {
  if (
    argument.trim() !== argument ||
    containsRepeatedSpace.test(argument)
  ) {
    return false;
  }
  try {
    parser.parse(argument);
    return true;
  } catch (e) {
    // jison generates parsers that throw errors, while this function
    // mimics `semver.valid` by returning null.
    return null;
  }
};

// Comparison
// ----------

var ranges = require('./ranges.json');

var notALicenseIdentifier = ' is not a simple license identifier';

var rangeComparison = function(comparison) {
  return function(first, second) {
    var firstAST = exports.parse(first);
    if (!firstAST.hasOwnProperty('license')) {
      throw new Error('"' + first + '"' + notALicenseIdentifier);
    }
    var secondAST = exports.parse(second);
    if (!secondAST.hasOwnProperty('license')) {
      throw new Error('"' + second + '"' + notALicenseIdentifier);
    }
    return ranges.some(function(range) {
      var indexOfFirst = range.indexOf(firstAST.license);
      if (indexOfFirst < 0) {
        return false;
      }
      var indexOfSecond = range.indexOf(secondAST.license);
      if (indexOfSecond < 0) {
        return false;
      }
      return comparison(indexOfFirst, indexOfSecond);
    });
  };
};

exports.gt = rangeComparison(function(first, second) {
  return first > second;
});

exports.lt = rangeComparison(function(first, second) {
  return first < second;
});

exports.satisfies = (function() {
  var rangesAreCompatible = function(first, second) {
    return (
      first.license === second.license ||
      ranges.some(function(range) {
        return (
          range.indexOf(first.license) > -1 &&
          range.indexOf(second.license)
        );
      })
    );
  };

  var identifierInRange = function(identifier, range) {
    return (
      identifier.license === range.license ||
      exports.gt(identifier.license, range.license)
    );
  };

  var licensesAreCompatible = function(first, second) {
    if (first.exception !== second.exception) {
      return false;
    } else if (second.hasOwnProperty('license')) {
      if (second.hasOwnProperty('plus')) {
        if (first.hasOwnProperty('plus')) {
          // first+, second+
          return rangesAreCompatible(first, second);
        } else {
          // first, second+
          return identifierInRange(first, second);
        }
      } else {
        if (first.hasOwnProperty('plus')) {
          // first+, second
          return identifierInRange(second, first);
        } else {
          // first, second
          return first.license === second.license;
        }
      }
    }
  };

  var recurseLeftAndRight = function(first, second) {
    var firstConjunction = first.conjunction;
    if (firstConjunction === 'and') {
      return (
        recurse(first.left, second) &&
        recurse(first.right, second)
      );
    } else if (firstConjunction === 'or') {
      return (
        recurse(first.left, second) ||
        recurse(first.right, second)
      );
    }
  };

  var recurse = function(first, second) {
    if (first.hasOwnProperty('conjunction')) {
      return recurseLeftAndRight(first, second);
    } else if (second.hasOwnProperty('conjunction')) {
      return recurseLeftAndRight(second, first);
    } else {
      return licensesAreCompatible(first, second);
    }
  };

  return function(first, second) {
    return recurse(parser.parse(first), parser.parse(second));
  };
})();

// Reference Data
// --------------

// Require the same license and exception data used by the parser.
exports.licenses = require('spdx-license-ids');
exports.exceptions = require('./exceptions.json');

// Version Metadata
// ----------------

// The License Expression Syntax version
exports.specificationVersion = '2.0';

// This module's semantic version
exports.implementationVersion = '0.4.1';
