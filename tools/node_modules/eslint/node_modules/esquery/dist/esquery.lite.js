(function (global, factory) {
  typeof exports === 'object' && typeof module !== 'undefined' ? module.exports = factory(require('estraverse')) :
  typeof define === 'function' && define.amd ? define(['estraverse'], factory) :
  (global = global || self, global.esquery = factory(global.estraverse));
}(this, (function (estraverse) { 'use strict';

  estraverse = estraverse && Object.prototype.hasOwnProperty.call(estraverse, 'default') ? estraverse['default'] : estraverse;

  function _typeof(obj) {
    "@babel/helpers - typeof";

    if (typeof Symbol === "function" && typeof Symbol.iterator === "symbol") {
      _typeof = function (obj) {
        return typeof obj;
      };
    } else {
      _typeof = function (obj) {
        return obj && typeof Symbol === "function" && obj.constructor === Symbol && obj !== Symbol.prototype ? "symbol" : typeof obj;
      };
    }

    return _typeof(obj);
  }

  function _slicedToArray(arr, i) {
    return _arrayWithHoles(arr) || _iterableToArrayLimit(arr, i) || _unsupportedIterableToArray(arr, i) || _nonIterableRest();
  }

  function _toConsumableArray(arr) {
    return _arrayWithoutHoles(arr) || _iterableToArray(arr) || _unsupportedIterableToArray(arr) || _nonIterableSpread();
  }

  function _arrayWithoutHoles(arr) {
    if (Array.isArray(arr)) return _arrayLikeToArray(arr);
  }

  function _arrayWithHoles(arr) {
    if (Array.isArray(arr)) return arr;
  }

  function _iterableToArray(iter) {
    if (typeof Symbol !== "undefined" && Symbol.iterator in Object(iter)) return Array.from(iter);
  }

  function _iterableToArrayLimit(arr, i) {
    if (typeof Symbol === "undefined" || !(Symbol.iterator in Object(arr))) return;
    var _arr = [];
    var _n = true;
    var _d = false;
    var _e = undefined;

    try {
      for (var _i = arr[Symbol.iterator](), _s; !(_n = (_s = _i.next()).done); _n = true) {
        _arr.push(_s.value);

        if (i && _arr.length === i) break;
      }
    } catch (err) {
      _d = true;
      _e = err;
    } finally {
      try {
        if (!_n && _i["return"] != null) _i["return"]();
      } finally {
        if (_d) throw _e;
      }
    }

    return _arr;
  }

  function _unsupportedIterableToArray(o, minLen) {
    if (!o) return;
    if (typeof o === "string") return _arrayLikeToArray(o, minLen);
    var n = Object.prototype.toString.call(o).slice(8, -1);
    if (n === "Object" && o.constructor) n = o.constructor.name;
    if (n === "Map" || n === "Set") return Array.from(o);
    if (n === "Arguments" || /^(?:Ui|I)nt(?:8|16|32)(?:Clamped)?Array$/.test(n)) return _arrayLikeToArray(o, minLen);
  }

  function _arrayLikeToArray(arr, len) {
    if (len == null || len > arr.length) len = arr.length;

    for (var i = 0, arr2 = new Array(len); i < len; i++) arr2[i] = arr[i];

    return arr2;
  }

  function _nonIterableSpread() {
    throw new TypeError("Invalid attempt to spread non-iterable instance.\nIn order to be iterable, non-array objects must have a [Symbol.iterator]() method.");
  }

  function _nonIterableRest() {
    throw new TypeError("Invalid attempt to destructure non-iterable instance.\nIn order to be iterable, non-array objects must have a [Symbol.iterator]() method.");
  }

  function _createForOfIteratorHelper(o, allowArrayLike) {
    var it;

    if (typeof Symbol === "undefined" || o[Symbol.iterator] == null) {
      if (Array.isArray(o) || (it = _unsupportedIterableToArray(o)) || allowArrayLike && o && typeof o.length === "number") {
        if (it) o = it;
        var i = 0;

        var F = function () {};

        return {
          s: F,
          n: function () {
            if (i >= o.length) return {
              done: true
            };
            return {
              done: false,
              value: o[i++]
            };
          },
          e: function (e) {
            throw e;
          },
          f: F
        };
      }

      throw new TypeError("Invalid attempt to iterate non-iterable instance.\nIn order to be iterable, non-array objects must have a [Symbol.iterator]() method.");
    }

    var normalCompletion = true,
        didErr = false,
        err;
    return {
      s: function () {
        it = o[Symbol.iterator]();
      },
      n: function () {
        var step = it.next();
        normalCompletion = step.done;
        return step;
      },
      e: function (e) {
        didErr = true;
        err = e;
      },
      f: function () {
        try {
          if (!normalCompletion && it.return != null) it.return();
        } finally {
          if (didErr) throw err;
        }
      }
    };
  }

  var commonjsGlobal = typeof globalThis !== 'undefined' ? globalThis : typeof window !== 'undefined' ? window : typeof global !== 'undefined' ? global : typeof self !== 'undefined' ? self : {};

  function createCommonjsModule(fn, module) {
  	return module = { exports: {} }, fn(module, module.exports), module.exports;
  }

  var parser = createCommonjsModule(function (module) {
    /*
     * Generated by PEG.js 0.10.0.
     *
     * http://pegjs.org/
     */
    (function (root, factory) {
      if ( module.exports) {
        module.exports = factory();
      }
    })(commonjsGlobal, function () {

      function peg$subclass(child, parent) {
        function ctor() {
          this.constructor = child;
        }

        ctor.prototype = parent.prototype;
        child.prototype = new ctor();
      }

      function peg$SyntaxError(message, expected, found, location) {
        this.message = message;
        this.expected = expected;
        this.found = found;
        this.location = location;
        this.name = "SyntaxError";

        if (typeof Error.captureStackTrace === "function") {
          Error.captureStackTrace(this, peg$SyntaxError);
        }
      }

      peg$subclass(peg$SyntaxError, Error);

      peg$SyntaxError.buildMessage = function (expected, found) {
        var DESCRIBE_EXPECTATION_FNS = {
          literal: function literal(expectation) {
            return "\"" + literalEscape(expectation.text) + "\"";
          },
          "class": function _class(expectation) {
            var escapedParts = "",
                i;

            for (i = 0; i < expectation.parts.length; i++) {
              escapedParts += expectation.parts[i] instanceof Array ? classEscape(expectation.parts[i][0]) + "-" + classEscape(expectation.parts[i][1]) : classEscape(expectation.parts[i]);
            }

            return "[" + (expectation.inverted ? "^" : "") + escapedParts + "]";
          },
          any: function any(expectation) {
            return "any character";
          },
          end: function end(expectation) {
            return "end of input";
          },
          other: function other(expectation) {
            return expectation.description;
          }
        };

        function hex(ch) {
          return ch.charCodeAt(0).toString(16).toUpperCase();
        }

        function literalEscape(s) {
          return s.replace(/\\/g, '\\\\').replace(/"/g, '\\"').replace(/\0/g, '\\0').replace(/\t/g, '\\t').replace(/\n/g, '\\n').replace(/\r/g, '\\r').replace(/[\x00-\x0F]/g, function (ch) {
            return '\\x0' + hex(ch);
          }).replace(/[\x10-\x1F\x7F-\x9F]/g, function (ch) {
            return '\\x' + hex(ch);
          });
        }

        function classEscape(s) {
          return s.replace(/\\/g, '\\\\').replace(/\]/g, '\\]').replace(/\^/g, '\\^').replace(/-/g, '\\-').replace(/\0/g, '\\0').replace(/\t/g, '\\t').replace(/\n/g, '\\n').replace(/\r/g, '\\r').replace(/[\x00-\x0F]/g, function (ch) {
            return '\\x0' + hex(ch);
          }).replace(/[\x10-\x1F\x7F-\x9F]/g, function (ch) {
            return '\\x' + hex(ch);
          });
        }

        function describeExpectation(expectation) {
          return DESCRIBE_EXPECTATION_FNS[expectation.type](expectation);
        }

        function describeExpected(expected) {
          var descriptions = new Array(expected.length),
              i,
              j;

          for (i = 0; i < expected.length; i++) {
            descriptions[i] = describeExpectation(expected[i]);
          }

          descriptions.sort();

          if (descriptions.length > 0) {
            for (i = 1, j = 1; i < descriptions.length; i++) {
              if (descriptions[i - 1] !== descriptions[i]) {
                descriptions[j] = descriptions[i];
                j++;
              }
            }

            descriptions.length = j;
          }

          switch (descriptions.length) {
            case 1:
              return descriptions[0];

            case 2:
              return descriptions[0] + " or " + descriptions[1];

            default:
              return descriptions.slice(0, -1).join(", ") + ", or " + descriptions[descriptions.length - 1];
          }
        }

        function describeFound(found) {
          return found ? "\"" + literalEscape(found) + "\"" : "end of input";
        }

        return "Expected " + describeExpected(expected) + " but " + describeFound(found) + " found.";
      };

      function peg$parse(input, options) {
        options = options !== void 0 ? options : {};

        var peg$FAILED = {},
            peg$startRuleFunctions = {
          start: peg$parsestart
        },
            peg$startRuleFunction = peg$parsestart,
            peg$c0 = function peg$c0(ss) {
          return ss.length === 1 ? ss[0] : {
            type: 'matches',
            selectors: ss
          };
        },
            peg$c1 = function peg$c1() {
          return void 0;
        },
            peg$c2 = " ",
            peg$c3 = peg$literalExpectation(" ", false),
            peg$c4 = /^[^ [\],():#!=><~+.]/,
            peg$c5 = peg$classExpectation([" ", "[", "]", ",", "(", ")", ":", "#", "!", "=", ">", "<", "~", "+", "."], true, false),
            peg$c6 = function peg$c6(i) {
          return i.join('');
        },
            peg$c7 = ">",
            peg$c8 = peg$literalExpectation(">", false),
            peg$c9 = function peg$c9() {
          return 'child';
        },
            peg$c10 = "~",
            peg$c11 = peg$literalExpectation("~", false),
            peg$c12 = function peg$c12() {
          return 'sibling';
        },
            peg$c13 = "+",
            peg$c14 = peg$literalExpectation("+", false),
            peg$c15 = function peg$c15() {
          return 'adjacent';
        },
            peg$c16 = function peg$c16() {
          return 'descendant';
        },
            peg$c17 = ",",
            peg$c18 = peg$literalExpectation(",", false),
            peg$c19 = function peg$c19(s, ss) {
          return [s].concat(ss.map(function (s) {
            return s[3];
          }));
        },
            peg$c20 = function peg$c20(a, ops) {
          return ops.reduce(function (memo, rhs) {
            return {
              type: rhs[0],
              left: memo,
              right: rhs[1]
            };
          }, a);
        },
            peg$c21 = "!",
            peg$c22 = peg$literalExpectation("!", false),
            peg$c23 = function peg$c23(subject, as) {
          var b = as.length === 1 ? as[0] : {
            type: 'compound',
            selectors: as
          };
          if (subject) b.subject = true;
          return b;
        },
            peg$c24 = "*",
            peg$c25 = peg$literalExpectation("*", false),
            peg$c26 = function peg$c26(a) {
          return {
            type: 'wildcard',
            value: a
          };
        },
            peg$c27 = "#",
            peg$c28 = peg$literalExpectation("#", false),
            peg$c29 = function peg$c29(i) {
          return {
            type: 'identifier',
            value: i
          };
        },
            peg$c30 = "[",
            peg$c31 = peg$literalExpectation("[", false),
            peg$c32 = "]",
            peg$c33 = peg$literalExpectation("]", false),
            peg$c34 = function peg$c34(v) {
          return v;
        },
            peg$c35 = /^[><!]/,
            peg$c36 = peg$classExpectation([">", "<", "!"], false, false),
            peg$c37 = "=",
            peg$c38 = peg$literalExpectation("=", false),
            peg$c39 = function peg$c39(a) {
          return (a || '') + '=';
        },
            peg$c40 = /^[><]/,
            peg$c41 = peg$classExpectation([">", "<"], false, false),
            peg$c42 = ".",
            peg$c43 = peg$literalExpectation(".", false),
            peg$c44 = function peg$c44(a, as) {
          return [].concat.apply([a], as).join('');
        },
            peg$c45 = function peg$c45(name, op, value) {
          return {
            type: 'attribute',
            name: name,
            operator: op,
            value: value
          };
        },
            peg$c46 = function peg$c46(name) {
          return {
            type: 'attribute',
            name: name
          };
        },
            peg$c47 = "\"",
            peg$c48 = peg$literalExpectation("\"", false),
            peg$c49 = /^[^\\"]/,
            peg$c50 = peg$classExpectation(["\\", "\""], true, false),
            peg$c51 = "\\",
            peg$c52 = peg$literalExpectation("\\", false),
            peg$c53 = peg$anyExpectation(),
            peg$c54 = function peg$c54(a, b) {
          return a + b;
        },
            peg$c55 = function peg$c55(d) {
          return {
            type: 'literal',
            value: strUnescape(d.join(''))
          };
        },
            peg$c56 = "'",
            peg$c57 = peg$literalExpectation("'", false),
            peg$c58 = /^[^\\']/,
            peg$c59 = peg$classExpectation(["\\", "'"], true, false),
            peg$c60 = /^[0-9]/,
            peg$c61 = peg$classExpectation([["0", "9"]], false, false),
            peg$c62 = function peg$c62(a, b) {
          // Can use `a.flat().join('')` once supported
          var leadingDecimals = a ? [].concat.apply([], a).join('') : '';
          return {
            type: 'literal',
            value: parseFloat(leadingDecimals + b.join(''))
          };
        },
            peg$c63 = function peg$c63(i) {
          return {
            type: 'literal',
            value: i
          };
        },
            peg$c64 = "type(",
            peg$c65 = peg$literalExpectation("type(", false),
            peg$c66 = /^[^ )]/,
            peg$c67 = peg$classExpectation([" ", ")"], true, false),
            peg$c68 = ")",
            peg$c69 = peg$literalExpectation(")", false),
            peg$c70 = function peg$c70(t) {
          return {
            type: 'type',
            value: t.join('')
          };
        },
            peg$c71 = /^[imsu]/,
            peg$c72 = peg$classExpectation(["i", "m", "s", "u"], false, false),
            peg$c73 = "/",
            peg$c74 = peg$literalExpectation("/", false),
            peg$c75 = /^[^\/]/,
            peg$c76 = peg$classExpectation(["/"], true, false),
            peg$c77 = function peg$c77(d, flgs) {
          return {
            type: 'regexp',
            value: new RegExp(d.join(''), flgs ? flgs.join('') : '')
          };
        },
            peg$c78 = function peg$c78(i, is) {
          return {
            type: 'field',
            name: is.reduce(function (memo, p) {
              return memo + p[0] + p[1];
            }, i)
          };
        },
            peg$c79 = ":not(",
            peg$c80 = peg$literalExpectation(":not(", false),
            peg$c81 = function peg$c81(ss) {
          return {
            type: 'not',
            selectors: ss
          };
        },
            peg$c82 = ":matches(",
            peg$c83 = peg$literalExpectation(":matches(", false),
            peg$c84 = function peg$c84(ss) {
          return {
            type: 'matches',
            selectors: ss
          };
        },
            peg$c85 = ":has(",
            peg$c86 = peg$literalExpectation(":has(", false),
            peg$c87 = function peg$c87(ss) {
          return {
            type: 'has',
            selectors: ss
          };
        },
            peg$c88 = ":first-child",
            peg$c89 = peg$literalExpectation(":first-child", false),
            peg$c90 = function peg$c90() {
          return nth(1);
        },
            peg$c91 = ":last-child",
            peg$c92 = peg$literalExpectation(":last-child", false),
            peg$c93 = function peg$c93() {
          return nthLast(1);
        },
            peg$c94 = ":nth-child(",
            peg$c95 = peg$literalExpectation(":nth-child(", false),
            peg$c96 = function peg$c96(n) {
          return nth(parseInt(n.join(''), 10));
        },
            peg$c97 = ":nth-last-child(",
            peg$c98 = peg$literalExpectation(":nth-last-child(", false),
            peg$c99 = function peg$c99(n) {
          return nthLast(parseInt(n.join(''), 10));
        },
            peg$c100 = ":",
            peg$c101 = peg$literalExpectation(":", false),
            peg$c102 = "statement",
            peg$c103 = peg$literalExpectation("statement", true),
            peg$c104 = "expression",
            peg$c105 = peg$literalExpectation("expression", true),
            peg$c106 = "declaration",
            peg$c107 = peg$literalExpectation("declaration", true),
            peg$c108 = "function",
            peg$c109 = peg$literalExpectation("function", true),
            peg$c110 = "pattern",
            peg$c111 = peg$literalExpectation("pattern", true),
            peg$c112 = function peg$c112(c) {
          return {
            type: 'class',
            name: c
          };
        },
            peg$currPos = 0,
            peg$posDetailsCache = [{
          line: 1,
          column: 1
        }],
            peg$maxFailPos = 0,
            peg$maxFailExpected = [],
            peg$resultsCache = {},
            peg$result;

        if ("startRule" in options) {
          if (!(options.startRule in peg$startRuleFunctions)) {
            throw new Error("Can't start parsing from rule \"" + options.startRule + "\".");
          }

          peg$startRuleFunction = peg$startRuleFunctions[options.startRule];
        }

        function peg$literalExpectation(text, ignoreCase) {
          return {
            type: "literal",
            text: text,
            ignoreCase: ignoreCase
          };
        }

        function peg$classExpectation(parts, inverted, ignoreCase) {
          return {
            type: "class",
            parts: parts,
            inverted: inverted,
            ignoreCase: ignoreCase
          };
        }

        function peg$anyExpectation() {
          return {
            type: "any"
          };
        }

        function peg$endExpectation() {
          return {
            type: "end"
          };
        }

        function peg$computePosDetails(pos) {
          var details = peg$posDetailsCache[pos],
              p;

          if (details) {
            return details;
          } else {
            p = pos - 1;

            while (!peg$posDetailsCache[p]) {
              p--;
            }

            details = peg$posDetailsCache[p];
            details = {
              line: details.line,
              column: details.column
            };

            while (p < pos) {
              if (input.charCodeAt(p) === 10) {
                details.line++;
                details.column = 1;
              } else {
                details.column++;
              }

              p++;
            }

            peg$posDetailsCache[pos] = details;
            return details;
          }
        }

        function peg$computeLocation(startPos, endPos) {
          var startPosDetails = peg$computePosDetails(startPos),
              endPosDetails = peg$computePosDetails(endPos);
          return {
            start: {
              offset: startPos,
              line: startPosDetails.line,
              column: startPosDetails.column
            },
            end: {
              offset: endPos,
              line: endPosDetails.line,
              column: endPosDetails.column
            }
          };
        }

        function peg$fail(expected) {
          if (peg$currPos < peg$maxFailPos) {
            return;
          }

          if (peg$currPos > peg$maxFailPos) {
            peg$maxFailPos = peg$currPos;
            peg$maxFailExpected = [];
          }

          peg$maxFailExpected.push(expected);
        }

        function peg$buildStructuredError(expected, found, location) {
          return new peg$SyntaxError(peg$SyntaxError.buildMessage(expected, found), expected, found, location);
        }

        function peg$parsestart() {
          var s0, s1, s2, s3;
          var key = peg$currPos * 30 + 0,
              cached = peg$resultsCache[key];

          if (cached) {
            peg$currPos = cached.nextPos;
            return cached.result;
          }

          s0 = peg$currPos;
          s1 = peg$parse_();

          if (s1 !== peg$FAILED) {
            s2 = peg$parseselectors();

            if (s2 !== peg$FAILED) {
              s3 = peg$parse_();

              if (s3 !== peg$FAILED) {
                s1 = peg$c0(s2);
                s0 = s1;
              } else {
                peg$currPos = s0;
                s0 = peg$FAILED;
              }
            } else {
              peg$currPos = s0;
              s0 = peg$FAILED;
            }
          } else {
            peg$currPos = s0;
            s0 = peg$FAILED;
          }

          if (s0 === peg$FAILED) {
            s0 = peg$currPos;
            s1 = peg$parse_();

            if (s1 !== peg$FAILED) {
              s1 = peg$c1();
            }

            s0 = s1;
          }

          peg$resultsCache[key] = {
            nextPos: peg$currPos,
            result: s0
          };
          return s0;
        }

        function peg$parse_() {
          var s0, s1;
          var key = peg$currPos * 30 + 1,
              cached = peg$resultsCache[key];

          if (cached) {
            peg$currPos = cached.nextPos;
            return cached.result;
          }

          s0 = [];

          if (input.charCodeAt(peg$currPos) === 32) {
            s1 = peg$c2;
            peg$currPos++;
          } else {
            s1 = peg$FAILED;

            {
              peg$fail(peg$c3);
            }
          }

          while (s1 !== peg$FAILED) {
            s0.push(s1);

            if (input.charCodeAt(peg$currPos) === 32) {
              s1 = peg$c2;
              peg$currPos++;
            } else {
              s1 = peg$FAILED;

              {
                peg$fail(peg$c3);
              }
            }
          }

          peg$resultsCache[key] = {
            nextPos: peg$currPos,
            result: s0
          };
          return s0;
        }

        function peg$parseidentifierName() {
          var s0, s1, s2;
          var key = peg$currPos * 30 + 2,
              cached = peg$resultsCache[key];

          if (cached) {
            peg$currPos = cached.nextPos;
            return cached.result;
          }

          s0 = peg$currPos;
          s1 = [];

          if (peg$c4.test(input.charAt(peg$currPos))) {
            s2 = input.charAt(peg$currPos);
            peg$currPos++;
          } else {
            s2 = peg$FAILED;

            {
              peg$fail(peg$c5);
            }
          }

          if (s2 !== peg$FAILED) {
            while (s2 !== peg$FAILED) {
              s1.push(s2);

              if (peg$c4.test(input.charAt(peg$currPos))) {
                s2 = input.charAt(peg$currPos);
                peg$currPos++;
              } else {
                s2 = peg$FAILED;

                {
                  peg$fail(peg$c5);
                }
              }
            }
          } else {
            s1 = peg$FAILED;
          }

          if (s1 !== peg$FAILED) {
            s1 = peg$c6(s1);
          }

          s0 = s1;
          peg$resultsCache[key] = {
            nextPos: peg$currPos,
            result: s0
          };
          return s0;
        }

        function peg$parsebinaryOp() {
          var s0, s1, s2, s3;
          var key = peg$currPos * 30 + 3,
              cached = peg$resultsCache[key];

          if (cached) {
            peg$currPos = cached.nextPos;
            return cached.result;
          }

          s0 = peg$currPos;
          s1 = peg$parse_();

          if (s1 !== peg$FAILED) {
            if (input.charCodeAt(peg$currPos) === 62) {
              s2 = peg$c7;
              peg$currPos++;
            } else {
              s2 = peg$FAILED;

              {
                peg$fail(peg$c8);
              }
            }

            if (s2 !== peg$FAILED) {
              s3 = peg$parse_();

              if (s3 !== peg$FAILED) {
                s1 = peg$c9();
                s0 = s1;
              } else {
                peg$currPos = s0;
                s0 = peg$FAILED;
              }
            } else {
              peg$currPos = s0;
              s0 = peg$FAILED;
            }
          } else {
            peg$currPos = s0;
            s0 = peg$FAILED;
          }

          if (s0 === peg$FAILED) {
            s0 = peg$currPos;
            s1 = peg$parse_();

            if (s1 !== peg$FAILED) {
              if (input.charCodeAt(peg$currPos) === 126) {
                s2 = peg$c10;
                peg$currPos++;
              } else {
                s2 = peg$FAILED;

                {
                  peg$fail(peg$c11);
                }
              }

              if (s2 !== peg$FAILED) {
                s3 = peg$parse_();

                if (s3 !== peg$FAILED) {
                  s1 = peg$c12();
                  s0 = s1;
                } else {
                  peg$currPos = s0;
                  s0 = peg$FAILED;
                }
              } else {
                peg$currPos = s0;
                s0 = peg$FAILED;
              }
            } else {
              peg$currPos = s0;
              s0 = peg$FAILED;
            }

            if (s0 === peg$FAILED) {
              s0 = peg$currPos;
              s1 = peg$parse_();

              if (s1 !== peg$FAILED) {
                if (input.charCodeAt(peg$currPos) === 43) {
                  s2 = peg$c13;
                  peg$currPos++;
                } else {
                  s2 = peg$FAILED;

                  {
                    peg$fail(peg$c14);
                  }
                }

                if (s2 !== peg$FAILED) {
                  s3 = peg$parse_();

                  if (s3 !== peg$FAILED) {
                    s1 = peg$c15();
                    s0 = s1;
                  } else {
                    peg$currPos = s0;
                    s0 = peg$FAILED;
                  }
                } else {
                  peg$currPos = s0;
                  s0 = peg$FAILED;
                }
              } else {
                peg$currPos = s0;
                s0 = peg$FAILED;
              }

              if (s0 === peg$FAILED) {
                s0 = peg$currPos;

                if (input.charCodeAt(peg$currPos) === 32) {
                  s1 = peg$c2;
                  peg$currPos++;
                } else {
                  s1 = peg$FAILED;

                  {
                    peg$fail(peg$c3);
                  }
                }

                if (s1 !== peg$FAILED) {
                  s2 = peg$parse_();

                  if (s2 !== peg$FAILED) {
                    s1 = peg$c16();
                    s0 = s1;
                  } else {
                    peg$currPos = s0;
                    s0 = peg$FAILED;
                  }
                } else {
                  peg$currPos = s0;
                  s0 = peg$FAILED;
                }
              }
            }
          }

          peg$resultsCache[key] = {
            nextPos: peg$currPos,
            result: s0
          };
          return s0;
        }

        function peg$parseselectors() {
          var s0, s1, s2, s3, s4, s5, s6, s7;
          var key = peg$currPos * 30 + 4,
              cached = peg$resultsCache[key];

          if (cached) {
            peg$currPos = cached.nextPos;
            return cached.result;
          }

          s0 = peg$currPos;
          s1 = peg$parseselector();

          if (s1 !== peg$FAILED) {
            s2 = [];
            s3 = peg$currPos;
            s4 = peg$parse_();

            if (s4 !== peg$FAILED) {
              if (input.charCodeAt(peg$currPos) === 44) {
                s5 = peg$c17;
                peg$currPos++;
              } else {
                s5 = peg$FAILED;

                {
                  peg$fail(peg$c18);
                }
              }

              if (s5 !== peg$FAILED) {
                s6 = peg$parse_();

                if (s6 !== peg$FAILED) {
                  s7 = peg$parseselector();

                  if (s7 !== peg$FAILED) {
                    s4 = [s4, s5, s6, s7];
                    s3 = s4;
                  } else {
                    peg$currPos = s3;
                    s3 = peg$FAILED;
                  }
                } else {
                  peg$currPos = s3;
                  s3 = peg$FAILED;
                }
              } else {
                peg$currPos = s3;
                s3 = peg$FAILED;
              }
            } else {
              peg$currPos = s3;
              s3 = peg$FAILED;
            }

            while (s3 !== peg$FAILED) {
              s2.push(s3);
              s3 = peg$currPos;
              s4 = peg$parse_();

              if (s4 !== peg$FAILED) {
                if (input.charCodeAt(peg$currPos) === 44) {
                  s5 = peg$c17;
                  peg$currPos++;
                } else {
                  s5 = peg$FAILED;

                  {
                    peg$fail(peg$c18);
                  }
                }

                if (s5 !== peg$FAILED) {
                  s6 = peg$parse_();

                  if (s6 !== peg$FAILED) {
                    s7 = peg$parseselector();

                    if (s7 !== peg$FAILED) {
                      s4 = [s4, s5, s6, s7];
                      s3 = s4;
                    } else {
                      peg$currPos = s3;
                      s3 = peg$FAILED;
                    }
                  } else {
                    peg$currPos = s3;
                    s3 = peg$FAILED;
                  }
                } else {
                  peg$currPos = s3;
                  s3 = peg$FAILED;
                }
              } else {
                peg$currPos = s3;
                s3 = peg$FAILED;
              }
            }

            if (s2 !== peg$FAILED) {
              s1 = peg$c19(s1, s2);
              s0 = s1;
            } else {
              peg$currPos = s0;
              s0 = peg$FAILED;
            }
          } else {
            peg$currPos = s0;
            s0 = peg$FAILED;
          }

          peg$resultsCache[key] = {
            nextPos: peg$currPos,
            result: s0
          };
          return s0;
        }

        function peg$parseselector() {
          var s0, s1, s2, s3, s4, s5;
          var key = peg$currPos * 30 + 5,
              cached = peg$resultsCache[key];

          if (cached) {
            peg$currPos = cached.nextPos;
            return cached.result;
          }

          s0 = peg$currPos;
          s1 = peg$parsesequence();

          if (s1 !== peg$FAILED) {
            s2 = [];
            s3 = peg$currPos;
            s4 = peg$parsebinaryOp();

            if (s4 !== peg$FAILED) {
              s5 = peg$parsesequence();

              if (s5 !== peg$FAILED) {
                s4 = [s4, s5];
                s3 = s4;
              } else {
                peg$currPos = s3;
                s3 = peg$FAILED;
              }
            } else {
              peg$currPos = s3;
              s3 = peg$FAILED;
            }

            while (s3 !== peg$FAILED) {
              s2.push(s3);
              s3 = peg$currPos;
              s4 = peg$parsebinaryOp();

              if (s4 !== peg$FAILED) {
                s5 = peg$parsesequence();

                if (s5 !== peg$FAILED) {
                  s4 = [s4, s5];
                  s3 = s4;
                } else {
                  peg$currPos = s3;
                  s3 = peg$FAILED;
                }
              } else {
                peg$currPos = s3;
                s3 = peg$FAILED;
              }
            }

            if (s2 !== peg$FAILED) {
              s1 = peg$c20(s1, s2);
              s0 = s1;
            } else {
              peg$currPos = s0;
              s0 = peg$FAILED;
            }
          } else {
            peg$currPos = s0;
            s0 = peg$FAILED;
          }

          peg$resultsCache[key] = {
            nextPos: peg$currPos,
            result: s0
          };
          return s0;
        }

        function peg$parsesequence() {
          var s0, s1, s2, s3;
          var key = peg$currPos * 30 + 6,
              cached = peg$resultsCache[key];

          if (cached) {
            peg$currPos = cached.nextPos;
            return cached.result;
          }

          s0 = peg$currPos;

          if (input.charCodeAt(peg$currPos) === 33) {
            s1 = peg$c21;
            peg$currPos++;
          } else {
            s1 = peg$FAILED;

            {
              peg$fail(peg$c22);
            }
          }

          if (s1 === peg$FAILED) {
            s1 = null;
          }

          if (s1 !== peg$FAILED) {
            s2 = [];
            s3 = peg$parseatom();

            if (s3 !== peg$FAILED) {
              while (s3 !== peg$FAILED) {
                s2.push(s3);
                s3 = peg$parseatom();
              }
            } else {
              s2 = peg$FAILED;
            }

            if (s2 !== peg$FAILED) {
              s1 = peg$c23(s1, s2);
              s0 = s1;
            } else {
              peg$currPos = s0;
              s0 = peg$FAILED;
            }
          } else {
            peg$currPos = s0;
            s0 = peg$FAILED;
          }

          peg$resultsCache[key] = {
            nextPos: peg$currPos,
            result: s0
          };
          return s0;
        }

        function peg$parseatom() {
          var s0;
          var key = peg$currPos * 30 + 7,
              cached = peg$resultsCache[key];

          if (cached) {
            peg$currPos = cached.nextPos;
            return cached.result;
          }

          s0 = peg$parsewildcard();

          if (s0 === peg$FAILED) {
            s0 = peg$parseidentifier();

            if (s0 === peg$FAILED) {
              s0 = peg$parseattr();

              if (s0 === peg$FAILED) {
                s0 = peg$parsefield();

                if (s0 === peg$FAILED) {
                  s0 = peg$parsenegation();

                  if (s0 === peg$FAILED) {
                    s0 = peg$parsematches();

                    if (s0 === peg$FAILED) {
                      s0 = peg$parsehas();

                      if (s0 === peg$FAILED) {
                        s0 = peg$parsefirstChild();

                        if (s0 === peg$FAILED) {
                          s0 = peg$parselastChild();

                          if (s0 === peg$FAILED) {
                            s0 = peg$parsenthChild();

                            if (s0 === peg$FAILED) {
                              s0 = peg$parsenthLastChild();

                              if (s0 === peg$FAILED) {
                                s0 = peg$parseclass();
                              }
                            }
                          }
                        }
                      }
                    }
                  }
                }
              }
            }
          }

          peg$resultsCache[key] = {
            nextPos: peg$currPos,
            result: s0
          };
          return s0;
        }

        function peg$parsewildcard() {
          var s0, s1;
          var key = peg$currPos * 30 + 8,
              cached = peg$resultsCache[key];

          if (cached) {
            peg$currPos = cached.nextPos;
            return cached.result;
          }

          s0 = peg$currPos;

          if (input.charCodeAt(peg$currPos) === 42) {
            s1 = peg$c24;
            peg$currPos++;
          } else {
            s1 = peg$FAILED;

            {
              peg$fail(peg$c25);
            }
          }

          if (s1 !== peg$FAILED) {
            s1 = peg$c26(s1);
          }

          s0 = s1;
          peg$resultsCache[key] = {
            nextPos: peg$currPos,
            result: s0
          };
          return s0;
        }

        function peg$parseidentifier() {
          var s0, s1, s2;
          var key = peg$currPos * 30 + 9,
              cached = peg$resultsCache[key];

          if (cached) {
            peg$currPos = cached.nextPos;
            return cached.result;
          }

          s0 = peg$currPos;

          if (input.charCodeAt(peg$currPos) === 35) {
            s1 = peg$c27;
            peg$currPos++;
          } else {
            s1 = peg$FAILED;

            {
              peg$fail(peg$c28);
            }
          }

          if (s1 === peg$FAILED) {
            s1 = null;
          }

          if (s1 !== peg$FAILED) {
            s2 = peg$parseidentifierName();

            if (s2 !== peg$FAILED) {
              s1 = peg$c29(s2);
              s0 = s1;
            } else {
              peg$currPos = s0;
              s0 = peg$FAILED;
            }
          } else {
            peg$currPos = s0;
            s0 = peg$FAILED;
          }

          peg$resultsCache[key] = {
            nextPos: peg$currPos,
            result: s0
          };
          return s0;
        }

        function peg$parseattr() {
          var s0, s1, s2, s3, s4, s5;
          var key = peg$currPos * 30 + 10,
              cached = peg$resultsCache[key];

          if (cached) {
            peg$currPos = cached.nextPos;
            return cached.result;
          }

          s0 = peg$currPos;

          if (input.charCodeAt(peg$currPos) === 91) {
            s1 = peg$c30;
            peg$currPos++;
          } else {
            s1 = peg$FAILED;

            {
              peg$fail(peg$c31);
            }
          }

          if (s1 !== peg$FAILED) {
            s2 = peg$parse_();

            if (s2 !== peg$FAILED) {
              s3 = peg$parseattrValue();

              if (s3 !== peg$FAILED) {
                s4 = peg$parse_();

                if (s4 !== peg$FAILED) {
                  if (input.charCodeAt(peg$currPos) === 93) {
                    s5 = peg$c32;
                    peg$currPos++;
                  } else {
                    s5 = peg$FAILED;

                    {
                      peg$fail(peg$c33);
                    }
                  }

                  if (s5 !== peg$FAILED) {
                    s1 = peg$c34(s3);
                    s0 = s1;
                  } else {
                    peg$currPos = s0;
                    s0 = peg$FAILED;
                  }
                } else {
                  peg$currPos = s0;
                  s0 = peg$FAILED;
                }
              } else {
                peg$currPos = s0;
                s0 = peg$FAILED;
              }
            } else {
              peg$currPos = s0;
              s0 = peg$FAILED;
            }
          } else {
            peg$currPos = s0;
            s0 = peg$FAILED;
          }

          peg$resultsCache[key] = {
            nextPos: peg$currPos,
            result: s0
          };
          return s0;
        }

        function peg$parseattrOps() {
          var s0, s1, s2;
          var key = peg$currPos * 30 + 11,
              cached = peg$resultsCache[key];

          if (cached) {
            peg$currPos = cached.nextPos;
            return cached.result;
          }

          s0 = peg$currPos;

          if (peg$c35.test(input.charAt(peg$currPos))) {
            s1 = input.charAt(peg$currPos);
            peg$currPos++;
          } else {
            s1 = peg$FAILED;

            {
              peg$fail(peg$c36);
            }
          }

          if (s1 === peg$FAILED) {
            s1 = null;
          }

          if (s1 !== peg$FAILED) {
            if (input.charCodeAt(peg$currPos) === 61) {
              s2 = peg$c37;
              peg$currPos++;
            } else {
              s2 = peg$FAILED;

              {
                peg$fail(peg$c38);
              }
            }

            if (s2 !== peg$FAILED) {
              s1 = peg$c39(s1);
              s0 = s1;
            } else {
              peg$currPos = s0;
              s0 = peg$FAILED;
            }
          } else {
            peg$currPos = s0;
            s0 = peg$FAILED;
          }

          if (s0 === peg$FAILED) {
            if (peg$c40.test(input.charAt(peg$currPos))) {
              s0 = input.charAt(peg$currPos);
              peg$currPos++;
            } else {
              s0 = peg$FAILED;

              {
                peg$fail(peg$c41);
              }
            }
          }

          peg$resultsCache[key] = {
            nextPos: peg$currPos,
            result: s0
          };
          return s0;
        }

        function peg$parseattrEqOps() {
          var s0, s1, s2;
          var key = peg$currPos * 30 + 12,
              cached = peg$resultsCache[key];

          if (cached) {
            peg$currPos = cached.nextPos;
            return cached.result;
          }

          s0 = peg$currPos;

          if (input.charCodeAt(peg$currPos) === 33) {
            s1 = peg$c21;
            peg$currPos++;
          } else {
            s1 = peg$FAILED;

            {
              peg$fail(peg$c22);
            }
          }

          if (s1 === peg$FAILED) {
            s1 = null;
          }

          if (s1 !== peg$FAILED) {
            if (input.charCodeAt(peg$currPos) === 61) {
              s2 = peg$c37;
              peg$currPos++;
            } else {
              s2 = peg$FAILED;

              {
                peg$fail(peg$c38);
              }
            }

            if (s2 !== peg$FAILED) {
              s1 = peg$c39(s1);
              s0 = s1;
            } else {
              peg$currPos = s0;
              s0 = peg$FAILED;
            }
          } else {
            peg$currPos = s0;
            s0 = peg$FAILED;
          }

          peg$resultsCache[key] = {
            nextPos: peg$currPos,
            result: s0
          };
          return s0;
        }

        function peg$parseattrName() {
          var s0, s1, s2, s3, s4, s5;
          var key = peg$currPos * 30 + 13,
              cached = peg$resultsCache[key];

          if (cached) {
            peg$currPos = cached.nextPos;
            return cached.result;
          }

          s0 = peg$currPos;
          s1 = peg$parseidentifierName();

          if (s1 !== peg$FAILED) {
            s2 = [];
            s3 = peg$currPos;

            if (input.charCodeAt(peg$currPos) === 46) {
              s4 = peg$c42;
              peg$currPos++;
            } else {
              s4 = peg$FAILED;

              {
                peg$fail(peg$c43);
              }
            }

            if (s4 !== peg$FAILED) {
              s5 = peg$parseidentifierName();

              if (s5 !== peg$FAILED) {
                s4 = [s4, s5];
                s3 = s4;
              } else {
                peg$currPos = s3;
                s3 = peg$FAILED;
              }
            } else {
              peg$currPos = s3;
              s3 = peg$FAILED;
            }

            while (s3 !== peg$FAILED) {
              s2.push(s3);
              s3 = peg$currPos;

              if (input.charCodeAt(peg$currPos) === 46) {
                s4 = peg$c42;
                peg$currPos++;
              } else {
                s4 = peg$FAILED;

                {
                  peg$fail(peg$c43);
                }
              }

              if (s4 !== peg$FAILED) {
                s5 = peg$parseidentifierName();

                if (s5 !== peg$FAILED) {
                  s4 = [s4, s5];
                  s3 = s4;
                } else {
                  peg$currPos = s3;
                  s3 = peg$FAILED;
                }
              } else {
                peg$currPos = s3;
                s3 = peg$FAILED;
              }
            }

            if (s2 !== peg$FAILED) {
              s1 = peg$c44(s1, s2);
              s0 = s1;
            } else {
              peg$currPos = s0;
              s0 = peg$FAILED;
            }
          } else {
            peg$currPos = s0;
            s0 = peg$FAILED;
          }

          peg$resultsCache[key] = {
            nextPos: peg$currPos,
            result: s0
          };
          return s0;
        }

        function peg$parseattrValue() {
          var s0, s1, s2, s3, s4, s5;
          var key = peg$currPos * 30 + 14,
              cached = peg$resultsCache[key];

          if (cached) {
            peg$currPos = cached.nextPos;
            return cached.result;
          }

          s0 = peg$currPos;
          s1 = peg$parseattrName();

          if (s1 !== peg$FAILED) {
            s2 = peg$parse_();

            if (s2 !== peg$FAILED) {
              s3 = peg$parseattrEqOps();

              if (s3 !== peg$FAILED) {
                s4 = peg$parse_();

                if (s4 !== peg$FAILED) {
                  s5 = peg$parsetype();

                  if (s5 === peg$FAILED) {
                    s5 = peg$parseregex();
                  }

                  if (s5 !== peg$FAILED) {
                    s1 = peg$c45(s1, s3, s5);
                    s0 = s1;
                  } else {
                    peg$currPos = s0;
                    s0 = peg$FAILED;
                  }
                } else {
                  peg$currPos = s0;
                  s0 = peg$FAILED;
                }
              } else {
                peg$currPos = s0;
                s0 = peg$FAILED;
              }
            } else {
              peg$currPos = s0;
              s0 = peg$FAILED;
            }
          } else {
            peg$currPos = s0;
            s0 = peg$FAILED;
          }

          if (s0 === peg$FAILED) {
            s0 = peg$currPos;
            s1 = peg$parseattrName();

            if (s1 !== peg$FAILED) {
              s2 = peg$parse_();

              if (s2 !== peg$FAILED) {
                s3 = peg$parseattrOps();

                if (s3 !== peg$FAILED) {
                  s4 = peg$parse_();

                  if (s4 !== peg$FAILED) {
                    s5 = peg$parsestring();

                    if (s5 === peg$FAILED) {
                      s5 = peg$parsenumber();

                      if (s5 === peg$FAILED) {
                        s5 = peg$parsepath();
                      }
                    }

                    if (s5 !== peg$FAILED) {
                      s1 = peg$c45(s1, s3, s5);
                      s0 = s1;
                    } else {
                      peg$currPos = s0;
                      s0 = peg$FAILED;
                    }
                  } else {
                    peg$currPos = s0;
                    s0 = peg$FAILED;
                  }
                } else {
                  peg$currPos = s0;
                  s0 = peg$FAILED;
                }
              } else {
                peg$currPos = s0;
                s0 = peg$FAILED;
              }
            } else {
              peg$currPos = s0;
              s0 = peg$FAILED;
            }

            if (s0 === peg$FAILED) {
              s0 = peg$currPos;
              s1 = peg$parseattrName();

              if (s1 !== peg$FAILED) {
                s1 = peg$c46(s1);
              }

              s0 = s1;
            }
          }

          peg$resultsCache[key] = {
            nextPos: peg$currPos,
            result: s0
          };
          return s0;
        }

        function peg$parsestring() {
          var s0, s1, s2, s3, s4, s5;
          var key = peg$currPos * 30 + 15,
              cached = peg$resultsCache[key];

          if (cached) {
            peg$currPos = cached.nextPos;
            return cached.result;
          }

          s0 = peg$currPos;

          if (input.charCodeAt(peg$currPos) === 34) {
            s1 = peg$c47;
            peg$currPos++;
          } else {
            s1 = peg$FAILED;

            {
              peg$fail(peg$c48);
            }
          }

          if (s1 !== peg$FAILED) {
            s2 = [];

            if (peg$c49.test(input.charAt(peg$currPos))) {
              s3 = input.charAt(peg$currPos);
              peg$currPos++;
            } else {
              s3 = peg$FAILED;

              {
                peg$fail(peg$c50);
              }
            }

            if (s3 === peg$FAILED) {
              s3 = peg$currPos;

              if (input.charCodeAt(peg$currPos) === 92) {
                s4 = peg$c51;
                peg$currPos++;
              } else {
                s4 = peg$FAILED;

                {
                  peg$fail(peg$c52);
                }
              }

              if (s4 !== peg$FAILED) {
                if (input.length > peg$currPos) {
                  s5 = input.charAt(peg$currPos);
                  peg$currPos++;
                } else {
                  s5 = peg$FAILED;

                  {
                    peg$fail(peg$c53);
                  }
                }

                if (s5 !== peg$FAILED) {
                  s4 = peg$c54(s4, s5);
                  s3 = s4;
                } else {
                  peg$currPos = s3;
                  s3 = peg$FAILED;
                }
              } else {
                peg$currPos = s3;
                s3 = peg$FAILED;
              }
            }

            while (s3 !== peg$FAILED) {
              s2.push(s3);

              if (peg$c49.test(input.charAt(peg$currPos))) {
                s3 = input.charAt(peg$currPos);
                peg$currPos++;
              } else {
                s3 = peg$FAILED;

                {
                  peg$fail(peg$c50);
                }
              }

              if (s3 === peg$FAILED) {
                s3 = peg$currPos;

                if (input.charCodeAt(peg$currPos) === 92) {
                  s4 = peg$c51;
                  peg$currPos++;
                } else {
                  s4 = peg$FAILED;

                  {
                    peg$fail(peg$c52);
                  }
                }

                if (s4 !== peg$FAILED) {
                  if (input.length > peg$currPos) {
                    s5 = input.charAt(peg$currPos);
                    peg$currPos++;
                  } else {
                    s5 = peg$FAILED;

                    {
                      peg$fail(peg$c53);
                    }
                  }

                  if (s5 !== peg$FAILED) {
                    s4 = peg$c54(s4, s5);
                    s3 = s4;
                  } else {
                    peg$currPos = s3;
                    s3 = peg$FAILED;
                  }
                } else {
                  peg$currPos = s3;
                  s3 = peg$FAILED;
                }
              }
            }

            if (s2 !== peg$FAILED) {
              if (input.charCodeAt(peg$currPos) === 34) {
                s3 = peg$c47;
                peg$currPos++;
              } else {
                s3 = peg$FAILED;

                {
                  peg$fail(peg$c48);
                }
              }

              if (s3 !== peg$FAILED) {
                s1 = peg$c55(s2);
                s0 = s1;
              } else {
                peg$currPos = s0;
                s0 = peg$FAILED;
              }
            } else {
              peg$currPos = s0;
              s0 = peg$FAILED;
            }
          } else {
            peg$currPos = s0;
            s0 = peg$FAILED;
          }

          if (s0 === peg$FAILED) {
            s0 = peg$currPos;

            if (input.charCodeAt(peg$currPos) === 39) {
              s1 = peg$c56;
              peg$currPos++;
            } else {
              s1 = peg$FAILED;

              {
                peg$fail(peg$c57);
              }
            }

            if (s1 !== peg$FAILED) {
              s2 = [];

              if (peg$c58.test(input.charAt(peg$currPos))) {
                s3 = input.charAt(peg$currPos);
                peg$currPos++;
              } else {
                s3 = peg$FAILED;

                {
                  peg$fail(peg$c59);
                }
              }

              if (s3 === peg$FAILED) {
                s3 = peg$currPos;

                if (input.charCodeAt(peg$currPos) === 92) {
                  s4 = peg$c51;
                  peg$currPos++;
                } else {
                  s4 = peg$FAILED;

                  {
                    peg$fail(peg$c52);
                  }
                }

                if (s4 !== peg$FAILED) {
                  if (input.length > peg$currPos) {
                    s5 = input.charAt(peg$currPos);
                    peg$currPos++;
                  } else {
                    s5 = peg$FAILED;

                    {
                      peg$fail(peg$c53);
                    }
                  }

                  if (s5 !== peg$FAILED) {
                    s4 = peg$c54(s4, s5);
                    s3 = s4;
                  } else {
                    peg$currPos = s3;
                    s3 = peg$FAILED;
                  }
                } else {
                  peg$currPos = s3;
                  s3 = peg$FAILED;
                }
              }

              while (s3 !== peg$FAILED) {
                s2.push(s3);

                if (peg$c58.test(input.charAt(peg$currPos))) {
                  s3 = input.charAt(peg$currPos);
                  peg$currPos++;
                } else {
                  s3 = peg$FAILED;

                  {
                    peg$fail(peg$c59);
                  }
                }

                if (s3 === peg$FAILED) {
                  s3 = peg$currPos;

                  if (input.charCodeAt(peg$currPos) === 92) {
                    s4 = peg$c51;
                    peg$currPos++;
                  } else {
                    s4 = peg$FAILED;

                    {
                      peg$fail(peg$c52);
                    }
                  }

                  if (s4 !== peg$FAILED) {
                    if (input.length > peg$currPos) {
                      s5 = input.charAt(peg$currPos);
                      peg$currPos++;
                    } else {
                      s5 = peg$FAILED;

                      {
                        peg$fail(peg$c53);
                      }
                    }

                    if (s5 !== peg$FAILED) {
                      s4 = peg$c54(s4, s5);
                      s3 = s4;
                    } else {
                      peg$currPos = s3;
                      s3 = peg$FAILED;
                    }
                  } else {
                    peg$currPos = s3;
                    s3 = peg$FAILED;
                  }
                }
              }

              if (s2 !== peg$FAILED) {
                if (input.charCodeAt(peg$currPos) === 39) {
                  s3 = peg$c56;
                  peg$currPos++;
                } else {
                  s3 = peg$FAILED;

                  {
                    peg$fail(peg$c57);
                  }
                }

                if (s3 !== peg$FAILED) {
                  s1 = peg$c55(s2);
                  s0 = s1;
                } else {
                  peg$currPos = s0;
                  s0 = peg$FAILED;
                }
              } else {
                peg$currPos = s0;
                s0 = peg$FAILED;
              }
            } else {
              peg$currPos = s0;
              s0 = peg$FAILED;
            }
          }

          peg$resultsCache[key] = {
            nextPos: peg$currPos,
            result: s0
          };
          return s0;
        }

        function peg$parsenumber() {
          var s0, s1, s2, s3;
          var key = peg$currPos * 30 + 16,
              cached = peg$resultsCache[key];

          if (cached) {
            peg$currPos = cached.nextPos;
            return cached.result;
          }

          s0 = peg$currPos;
          s1 = peg$currPos;
          s2 = [];

          if (peg$c60.test(input.charAt(peg$currPos))) {
            s3 = input.charAt(peg$currPos);
            peg$currPos++;
          } else {
            s3 = peg$FAILED;

            {
              peg$fail(peg$c61);
            }
          }

          while (s3 !== peg$FAILED) {
            s2.push(s3);

            if (peg$c60.test(input.charAt(peg$currPos))) {
              s3 = input.charAt(peg$currPos);
              peg$currPos++;
            } else {
              s3 = peg$FAILED;

              {
                peg$fail(peg$c61);
              }
            }
          }

          if (s2 !== peg$FAILED) {
            if (input.charCodeAt(peg$currPos) === 46) {
              s3 = peg$c42;
              peg$currPos++;
            } else {
              s3 = peg$FAILED;

              {
                peg$fail(peg$c43);
              }
            }

            if (s3 !== peg$FAILED) {
              s2 = [s2, s3];
              s1 = s2;
            } else {
              peg$currPos = s1;
              s1 = peg$FAILED;
            }
          } else {
            peg$currPos = s1;
            s1 = peg$FAILED;
          }

          if (s1 === peg$FAILED) {
            s1 = null;
          }

          if (s1 !== peg$FAILED) {
            s2 = [];

            if (peg$c60.test(input.charAt(peg$currPos))) {
              s3 = input.charAt(peg$currPos);
              peg$currPos++;
            } else {
              s3 = peg$FAILED;

              {
                peg$fail(peg$c61);
              }
            }

            if (s3 !== peg$FAILED) {
              while (s3 !== peg$FAILED) {
                s2.push(s3);

                if (peg$c60.test(input.charAt(peg$currPos))) {
                  s3 = input.charAt(peg$currPos);
                  peg$currPos++;
                } else {
                  s3 = peg$FAILED;

                  {
                    peg$fail(peg$c61);
                  }
                }
              }
            } else {
              s2 = peg$FAILED;
            }

            if (s2 !== peg$FAILED) {
              s1 = peg$c62(s1, s2);
              s0 = s1;
            } else {
              peg$currPos = s0;
              s0 = peg$FAILED;
            }
          } else {
            peg$currPos = s0;
            s0 = peg$FAILED;
          }

          peg$resultsCache[key] = {
            nextPos: peg$currPos,
            result: s0
          };
          return s0;
        }

        function peg$parsepath() {
          var s0, s1;
          var key = peg$currPos * 30 + 17,
              cached = peg$resultsCache[key];

          if (cached) {
            peg$currPos = cached.nextPos;
            return cached.result;
          }

          s0 = peg$currPos;
          s1 = peg$parseidentifierName();

          if (s1 !== peg$FAILED) {
            s1 = peg$c63(s1);
          }

          s0 = s1;
          peg$resultsCache[key] = {
            nextPos: peg$currPos,
            result: s0
          };
          return s0;
        }

        function peg$parsetype() {
          var s0, s1, s2, s3, s4, s5;
          var key = peg$currPos * 30 + 18,
              cached = peg$resultsCache[key];

          if (cached) {
            peg$currPos = cached.nextPos;
            return cached.result;
          }

          s0 = peg$currPos;

          if (input.substr(peg$currPos, 5) === peg$c64) {
            s1 = peg$c64;
            peg$currPos += 5;
          } else {
            s1 = peg$FAILED;

            {
              peg$fail(peg$c65);
            }
          }

          if (s1 !== peg$FAILED) {
            s2 = peg$parse_();

            if (s2 !== peg$FAILED) {
              s3 = [];

              if (peg$c66.test(input.charAt(peg$currPos))) {
                s4 = input.charAt(peg$currPos);
                peg$currPos++;
              } else {
                s4 = peg$FAILED;

                {
                  peg$fail(peg$c67);
                }
              }

              if (s4 !== peg$FAILED) {
                while (s4 !== peg$FAILED) {
                  s3.push(s4);

                  if (peg$c66.test(input.charAt(peg$currPos))) {
                    s4 = input.charAt(peg$currPos);
                    peg$currPos++;
                  } else {
                    s4 = peg$FAILED;

                    {
                      peg$fail(peg$c67);
                    }
                  }
                }
              } else {
                s3 = peg$FAILED;
              }

              if (s3 !== peg$FAILED) {
                s4 = peg$parse_();

                if (s4 !== peg$FAILED) {
                  if (input.charCodeAt(peg$currPos) === 41) {
                    s5 = peg$c68;
                    peg$currPos++;
                  } else {
                    s5 = peg$FAILED;

                    {
                      peg$fail(peg$c69);
                    }
                  }

                  if (s5 !== peg$FAILED) {
                    s1 = peg$c70(s3);
                    s0 = s1;
                  } else {
                    peg$currPos = s0;
                    s0 = peg$FAILED;
                  }
                } else {
                  peg$currPos = s0;
                  s0 = peg$FAILED;
                }
              } else {
                peg$currPos = s0;
                s0 = peg$FAILED;
              }
            } else {
              peg$currPos = s0;
              s0 = peg$FAILED;
            }
          } else {
            peg$currPos = s0;
            s0 = peg$FAILED;
          }

          peg$resultsCache[key] = {
            nextPos: peg$currPos,
            result: s0
          };
          return s0;
        }

        function peg$parseflags() {
          var s0, s1;
          var key = peg$currPos * 30 + 19,
              cached = peg$resultsCache[key];

          if (cached) {
            peg$currPos = cached.nextPos;
            return cached.result;
          }

          s0 = [];

          if (peg$c71.test(input.charAt(peg$currPos))) {
            s1 = input.charAt(peg$currPos);
            peg$currPos++;
          } else {
            s1 = peg$FAILED;

            {
              peg$fail(peg$c72);
            }
          }

          if (s1 !== peg$FAILED) {
            while (s1 !== peg$FAILED) {
              s0.push(s1);

              if (peg$c71.test(input.charAt(peg$currPos))) {
                s1 = input.charAt(peg$currPos);
                peg$currPos++;
              } else {
                s1 = peg$FAILED;

                {
                  peg$fail(peg$c72);
                }
              }
            }
          } else {
            s0 = peg$FAILED;
          }

          peg$resultsCache[key] = {
            nextPos: peg$currPos,
            result: s0
          };
          return s0;
        }

        function peg$parseregex() {
          var s0, s1, s2, s3, s4;
          var key = peg$currPos * 30 + 20,
              cached = peg$resultsCache[key];

          if (cached) {
            peg$currPos = cached.nextPos;
            return cached.result;
          }

          s0 = peg$currPos;

          if (input.charCodeAt(peg$currPos) === 47) {
            s1 = peg$c73;
            peg$currPos++;
          } else {
            s1 = peg$FAILED;

            {
              peg$fail(peg$c74);
            }
          }

          if (s1 !== peg$FAILED) {
            s2 = [];

            if (peg$c75.test(input.charAt(peg$currPos))) {
              s3 = input.charAt(peg$currPos);
              peg$currPos++;
            } else {
              s3 = peg$FAILED;

              {
                peg$fail(peg$c76);
              }
            }

            if (s3 !== peg$FAILED) {
              while (s3 !== peg$FAILED) {
                s2.push(s3);

                if (peg$c75.test(input.charAt(peg$currPos))) {
                  s3 = input.charAt(peg$currPos);
                  peg$currPos++;
                } else {
                  s3 = peg$FAILED;

                  {
                    peg$fail(peg$c76);
                  }
                }
              }
            } else {
              s2 = peg$FAILED;
            }

            if (s2 !== peg$FAILED) {
              if (input.charCodeAt(peg$currPos) === 47) {
                s3 = peg$c73;
                peg$currPos++;
              } else {
                s3 = peg$FAILED;

                {
                  peg$fail(peg$c74);
                }
              }

              if (s3 !== peg$FAILED) {
                s4 = peg$parseflags();

                if (s4 === peg$FAILED) {
                  s4 = null;
                }

                if (s4 !== peg$FAILED) {
                  s1 = peg$c77(s2, s4);
                  s0 = s1;
                } else {
                  peg$currPos = s0;
                  s0 = peg$FAILED;
                }
              } else {
                peg$currPos = s0;
                s0 = peg$FAILED;
              }
            } else {
              peg$currPos = s0;
              s0 = peg$FAILED;
            }
          } else {
            peg$currPos = s0;
            s0 = peg$FAILED;
          }

          peg$resultsCache[key] = {
            nextPos: peg$currPos,
            result: s0
          };
          return s0;
        }

        function peg$parsefield() {
          var s0, s1, s2, s3, s4, s5, s6;
          var key = peg$currPos * 30 + 21,
              cached = peg$resultsCache[key];

          if (cached) {
            peg$currPos = cached.nextPos;
            return cached.result;
          }

          s0 = peg$currPos;

          if (input.charCodeAt(peg$currPos) === 46) {
            s1 = peg$c42;
            peg$currPos++;
          } else {
            s1 = peg$FAILED;

            {
              peg$fail(peg$c43);
            }
          }

          if (s1 !== peg$FAILED) {
            s2 = peg$parseidentifierName();

            if (s2 !== peg$FAILED) {
              s3 = [];
              s4 = peg$currPos;

              if (input.charCodeAt(peg$currPos) === 46) {
                s5 = peg$c42;
                peg$currPos++;
              } else {
                s5 = peg$FAILED;

                {
                  peg$fail(peg$c43);
                }
              }

              if (s5 !== peg$FAILED) {
                s6 = peg$parseidentifierName();

                if (s6 !== peg$FAILED) {
                  s5 = [s5, s6];
                  s4 = s5;
                } else {
                  peg$currPos = s4;
                  s4 = peg$FAILED;
                }
              } else {
                peg$currPos = s4;
                s4 = peg$FAILED;
              }

              while (s4 !== peg$FAILED) {
                s3.push(s4);
                s4 = peg$currPos;

                if (input.charCodeAt(peg$currPos) === 46) {
                  s5 = peg$c42;
                  peg$currPos++;
                } else {
                  s5 = peg$FAILED;

                  {
                    peg$fail(peg$c43);
                  }
                }

                if (s5 !== peg$FAILED) {
                  s6 = peg$parseidentifierName();

                  if (s6 !== peg$FAILED) {
                    s5 = [s5, s6];
                    s4 = s5;
                  } else {
                    peg$currPos = s4;
                    s4 = peg$FAILED;
                  }
                } else {
                  peg$currPos = s4;
                  s4 = peg$FAILED;
                }
              }

              if (s3 !== peg$FAILED) {
                s1 = peg$c78(s2, s3);
                s0 = s1;
              } else {
                peg$currPos = s0;
                s0 = peg$FAILED;
              }
            } else {
              peg$currPos = s0;
              s0 = peg$FAILED;
            }
          } else {
            peg$currPos = s0;
            s0 = peg$FAILED;
          }

          peg$resultsCache[key] = {
            nextPos: peg$currPos,
            result: s0
          };
          return s0;
        }

        function peg$parsenegation() {
          var s0, s1, s2, s3, s4, s5;
          var key = peg$currPos * 30 + 22,
              cached = peg$resultsCache[key];

          if (cached) {
            peg$currPos = cached.nextPos;
            return cached.result;
          }

          s0 = peg$currPos;

          if (input.substr(peg$currPos, 5) === peg$c79) {
            s1 = peg$c79;
            peg$currPos += 5;
          } else {
            s1 = peg$FAILED;

            {
              peg$fail(peg$c80);
            }
          }

          if (s1 !== peg$FAILED) {
            s2 = peg$parse_();

            if (s2 !== peg$FAILED) {
              s3 = peg$parseselectors();

              if (s3 !== peg$FAILED) {
                s4 = peg$parse_();

                if (s4 !== peg$FAILED) {
                  if (input.charCodeAt(peg$currPos) === 41) {
                    s5 = peg$c68;
                    peg$currPos++;
                  } else {
                    s5 = peg$FAILED;

                    {
                      peg$fail(peg$c69);
                    }
                  }

                  if (s5 !== peg$FAILED) {
                    s1 = peg$c81(s3);
                    s0 = s1;
                  } else {
                    peg$currPos = s0;
                    s0 = peg$FAILED;
                  }
                } else {
                  peg$currPos = s0;
                  s0 = peg$FAILED;
                }
              } else {
                peg$currPos = s0;
                s0 = peg$FAILED;
              }
            } else {
              peg$currPos = s0;
              s0 = peg$FAILED;
            }
          } else {
            peg$currPos = s0;
            s0 = peg$FAILED;
          }

          peg$resultsCache[key] = {
            nextPos: peg$currPos,
            result: s0
          };
          return s0;
        }

        function peg$parsematches() {
          var s0, s1, s2, s3, s4, s5;
          var key = peg$currPos * 30 + 23,
              cached = peg$resultsCache[key];

          if (cached) {
            peg$currPos = cached.nextPos;
            return cached.result;
          }

          s0 = peg$currPos;

          if (input.substr(peg$currPos, 9) === peg$c82) {
            s1 = peg$c82;
            peg$currPos += 9;
          } else {
            s1 = peg$FAILED;

            {
              peg$fail(peg$c83);
            }
          }

          if (s1 !== peg$FAILED) {
            s2 = peg$parse_();

            if (s2 !== peg$FAILED) {
              s3 = peg$parseselectors();

              if (s3 !== peg$FAILED) {
                s4 = peg$parse_();

                if (s4 !== peg$FAILED) {
                  if (input.charCodeAt(peg$currPos) === 41) {
                    s5 = peg$c68;
                    peg$currPos++;
                  } else {
                    s5 = peg$FAILED;

                    {
                      peg$fail(peg$c69);
                    }
                  }

                  if (s5 !== peg$FAILED) {
                    s1 = peg$c84(s3);
                    s0 = s1;
                  } else {
                    peg$currPos = s0;
                    s0 = peg$FAILED;
                  }
                } else {
                  peg$currPos = s0;
                  s0 = peg$FAILED;
                }
              } else {
                peg$currPos = s0;
                s0 = peg$FAILED;
              }
            } else {
              peg$currPos = s0;
              s0 = peg$FAILED;
            }
          } else {
            peg$currPos = s0;
            s0 = peg$FAILED;
          }

          peg$resultsCache[key] = {
            nextPos: peg$currPos,
            result: s0
          };
          return s0;
        }

        function peg$parsehas() {
          var s0, s1, s2, s3, s4, s5;
          var key = peg$currPos * 30 + 24,
              cached = peg$resultsCache[key];

          if (cached) {
            peg$currPos = cached.nextPos;
            return cached.result;
          }

          s0 = peg$currPos;

          if (input.substr(peg$currPos, 5) === peg$c85) {
            s1 = peg$c85;
            peg$currPos += 5;
          } else {
            s1 = peg$FAILED;

            {
              peg$fail(peg$c86);
            }
          }

          if (s1 !== peg$FAILED) {
            s2 = peg$parse_();

            if (s2 !== peg$FAILED) {
              s3 = peg$parseselectors();

              if (s3 !== peg$FAILED) {
                s4 = peg$parse_();

                if (s4 !== peg$FAILED) {
                  if (input.charCodeAt(peg$currPos) === 41) {
                    s5 = peg$c68;
                    peg$currPos++;
                  } else {
                    s5 = peg$FAILED;

                    {
                      peg$fail(peg$c69);
                    }
                  }

                  if (s5 !== peg$FAILED) {
                    s1 = peg$c87(s3);
                    s0 = s1;
                  } else {
                    peg$currPos = s0;
                    s0 = peg$FAILED;
                  }
                } else {
                  peg$currPos = s0;
                  s0 = peg$FAILED;
                }
              } else {
                peg$currPos = s0;
                s0 = peg$FAILED;
              }
            } else {
              peg$currPos = s0;
              s0 = peg$FAILED;
            }
          } else {
            peg$currPos = s0;
            s0 = peg$FAILED;
          }

          peg$resultsCache[key] = {
            nextPos: peg$currPos,
            result: s0
          };
          return s0;
        }

        function peg$parsefirstChild() {
          var s0, s1;
          var key = peg$currPos * 30 + 25,
              cached = peg$resultsCache[key];

          if (cached) {
            peg$currPos = cached.nextPos;
            return cached.result;
          }

          s0 = peg$currPos;

          if (input.substr(peg$currPos, 12) === peg$c88) {
            s1 = peg$c88;
            peg$currPos += 12;
          } else {
            s1 = peg$FAILED;

            {
              peg$fail(peg$c89);
            }
          }

          if (s1 !== peg$FAILED) {
            s1 = peg$c90();
          }

          s0 = s1;
          peg$resultsCache[key] = {
            nextPos: peg$currPos,
            result: s0
          };
          return s0;
        }

        function peg$parselastChild() {
          var s0, s1;
          var key = peg$currPos * 30 + 26,
              cached = peg$resultsCache[key];

          if (cached) {
            peg$currPos = cached.nextPos;
            return cached.result;
          }

          s0 = peg$currPos;

          if (input.substr(peg$currPos, 11) === peg$c91) {
            s1 = peg$c91;
            peg$currPos += 11;
          } else {
            s1 = peg$FAILED;

            {
              peg$fail(peg$c92);
            }
          }

          if (s1 !== peg$FAILED) {
            s1 = peg$c93();
          }

          s0 = s1;
          peg$resultsCache[key] = {
            nextPos: peg$currPos,
            result: s0
          };
          return s0;
        }

        function peg$parsenthChild() {
          var s0, s1, s2, s3, s4, s5;
          var key = peg$currPos * 30 + 27,
              cached = peg$resultsCache[key];

          if (cached) {
            peg$currPos = cached.nextPos;
            return cached.result;
          }

          s0 = peg$currPos;

          if (input.substr(peg$currPos, 11) === peg$c94) {
            s1 = peg$c94;
            peg$currPos += 11;
          } else {
            s1 = peg$FAILED;

            {
              peg$fail(peg$c95);
            }
          }

          if (s1 !== peg$FAILED) {
            s2 = peg$parse_();

            if (s2 !== peg$FAILED) {
              s3 = [];

              if (peg$c60.test(input.charAt(peg$currPos))) {
                s4 = input.charAt(peg$currPos);
                peg$currPos++;
              } else {
                s4 = peg$FAILED;

                {
                  peg$fail(peg$c61);
                }
              }

              if (s4 !== peg$FAILED) {
                while (s4 !== peg$FAILED) {
                  s3.push(s4);

                  if (peg$c60.test(input.charAt(peg$currPos))) {
                    s4 = input.charAt(peg$currPos);
                    peg$currPos++;
                  } else {
                    s4 = peg$FAILED;

                    {
                      peg$fail(peg$c61);
                    }
                  }
                }
              } else {
                s3 = peg$FAILED;
              }

              if (s3 !== peg$FAILED) {
                s4 = peg$parse_();

                if (s4 !== peg$FAILED) {
                  if (input.charCodeAt(peg$currPos) === 41) {
                    s5 = peg$c68;
                    peg$currPos++;
                  } else {
                    s5 = peg$FAILED;

                    {
                      peg$fail(peg$c69);
                    }
                  }

                  if (s5 !== peg$FAILED) {
                    s1 = peg$c96(s3);
                    s0 = s1;
                  } else {
                    peg$currPos = s0;
                    s0 = peg$FAILED;
                  }
                } else {
                  peg$currPos = s0;
                  s0 = peg$FAILED;
                }
              } else {
                peg$currPos = s0;
                s0 = peg$FAILED;
              }
            } else {
              peg$currPos = s0;
              s0 = peg$FAILED;
            }
          } else {
            peg$currPos = s0;
            s0 = peg$FAILED;
          }

          peg$resultsCache[key] = {
            nextPos: peg$currPos,
            result: s0
          };
          return s0;
        }

        function peg$parsenthLastChild() {
          var s0, s1, s2, s3, s4, s5;
          var key = peg$currPos * 30 + 28,
              cached = peg$resultsCache[key];

          if (cached) {
            peg$currPos = cached.nextPos;
            return cached.result;
          }

          s0 = peg$currPos;

          if (input.substr(peg$currPos, 16) === peg$c97) {
            s1 = peg$c97;
            peg$currPos += 16;
          } else {
            s1 = peg$FAILED;

            {
              peg$fail(peg$c98);
            }
          }

          if (s1 !== peg$FAILED) {
            s2 = peg$parse_();

            if (s2 !== peg$FAILED) {
              s3 = [];

              if (peg$c60.test(input.charAt(peg$currPos))) {
                s4 = input.charAt(peg$currPos);
                peg$currPos++;
              } else {
                s4 = peg$FAILED;

                {
                  peg$fail(peg$c61);
                }
              }

              if (s4 !== peg$FAILED) {
                while (s4 !== peg$FAILED) {
                  s3.push(s4);

                  if (peg$c60.test(input.charAt(peg$currPos))) {
                    s4 = input.charAt(peg$currPos);
                    peg$currPos++;
                  } else {
                    s4 = peg$FAILED;

                    {
                      peg$fail(peg$c61);
                    }
                  }
                }
              } else {
                s3 = peg$FAILED;
              }

              if (s3 !== peg$FAILED) {
                s4 = peg$parse_();

                if (s4 !== peg$FAILED) {
                  if (input.charCodeAt(peg$currPos) === 41) {
                    s5 = peg$c68;
                    peg$currPos++;
                  } else {
                    s5 = peg$FAILED;

                    {
                      peg$fail(peg$c69);
                    }
                  }

                  if (s5 !== peg$FAILED) {
                    s1 = peg$c99(s3);
                    s0 = s1;
                  } else {
                    peg$currPos = s0;
                    s0 = peg$FAILED;
                  }
                } else {
                  peg$currPos = s0;
                  s0 = peg$FAILED;
                }
              } else {
                peg$currPos = s0;
                s0 = peg$FAILED;
              }
            } else {
              peg$currPos = s0;
              s0 = peg$FAILED;
            }
          } else {
            peg$currPos = s0;
            s0 = peg$FAILED;
          }

          peg$resultsCache[key] = {
            nextPos: peg$currPos,
            result: s0
          };
          return s0;
        }

        function peg$parseclass() {
          var s0, s1, s2;
          var key = peg$currPos * 30 + 29,
              cached = peg$resultsCache[key];

          if (cached) {
            peg$currPos = cached.nextPos;
            return cached.result;
          }

          s0 = peg$currPos;

          if (input.charCodeAt(peg$currPos) === 58) {
            s1 = peg$c100;
            peg$currPos++;
          } else {
            s1 = peg$FAILED;

            {
              peg$fail(peg$c101);
            }
          }

          if (s1 !== peg$FAILED) {
            if (input.substr(peg$currPos, 9).toLowerCase() === peg$c102) {
              s2 = input.substr(peg$currPos, 9);
              peg$currPos += 9;
            } else {
              s2 = peg$FAILED;

              {
                peg$fail(peg$c103);
              }
            }

            if (s2 === peg$FAILED) {
              if (input.substr(peg$currPos, 10).toLowerCase() === peg$c104) {
                s2 = input.substr(peg$currPos, 10);
                peg$currPos += 10;
              } else {
                s2 = peg$FAILED;

                {
                  peg$fail(peg$c105);
                }
              }

              if (s2 === peg$FAILED) {
                if (input.substr(peg$currPos, 11).toLowerCase() === peg$c106) {
                  s2 = input.substr(peg$currPos, 11);
                  peg$currPos += 11;
                } else {
                  s2 = peg$FAILED;

                  {
                    peg$fail(peg$c107);
                  }
                }

                if (s2 === peg$FAILED) {
                  if (input.substr(peg$currPos, 8).toLowerCase() === peg$c108) {
                    s2 = input.substr(peg$currPos, 8);
                    peg$currPos += 8;
                  } else {
                    s2 = peg$FAILED;

                    {
                      peg$fail(peg$c109);
                    }
                  }

                  if (s2 === peg$FAILED) {
                    if (input.substr(peg$currPos, 7).toLowerCase() === peg$c110) {
                      s2 = input.substr(peg$currPos, 7);
                      peg$currPos += 7;
                    } else {
                      s2 = peg$FAILED;

                      {
                        peg$fail(peg$c111);
                      }
                    }
                  }
                }
              }
            }

            if (s2 !== peg$FAILED) {
              s1 = peg$c112(s2);
              s0 = s1;
            } else {
              peg$currPos = s0;
              s0 = peg$FAILED;
            }
          } else {
            peg$currPos = s0;
            s0 = peg$FAILED;
          }

          peg$resultsCache[key] = {
            nextPos: peg$currPos,
            result: s0
          };
          return s0;
        }

        function nth(n) {
          return {
            type: 'nth-child',
            index: {
              type: 'literal',
              value: n
            }
          };
        }

        function nthLast(n) {
          return {
            type: 'nth-last-child',
            index: {
              type: 'literal',
              value: n
            }
          };
        }

        function strUnescape(s) {
          return s.replace(/\\(.)/g, function (match, ch) {
            switch (ch) {
              case 'b':
                return '\b';

              case 'f':
                return '\f';

              case 'n':
                return '\n';

              case 'r':
                return '\r';

              case 't':
                return '\t';

              case 'v':
                return '\v';

              default:
                return ch;
            }
          });
        }

        peg$result = peg$startRuleFunction();

        if (peg$result !== peg$FAILED && peg$currPos === input.length) {
          return peg$result;
        } else {
          if (peg$result !== peg$FAILED && peg$currPos < input.length) {
            peg$fail(peg$endExpectation());
          }

          throw peg$buildStructuredError(peg$maxFailExpected, peg$maxFailPos < input.length ? input.charAt(peg$maxFailPos) : null, peg$maxFailPos < input.length ? peg$computeLocation(peg$maxFailPos, peg$maxFailPos + 1) : peg$computeLocation(peg$maxFailPos, peg$maxFailPos));
        }
      }

      return {
        SyntaxError: peg$SyntaxError,
        parse: peg$parse
      };
    });
  });

  function _objectEntries(obj) {
    var entries = [];
    var keys = Object.keys(obj);

    for (var k = 0; k < keys.length; k++) entries.push([keys[k], obj[keys[k]]]);

    return entries;
  }
  /**
  * @typedef {"LEFT_SIDE"|"RIGHT_SIDE"} Side
  */

  var LEFT_SIDE = 'LEFT_SIDE';
  var RIGHT_SIDE = 'RIGHT_SIDE';
  /**
   * @external AST
   * @see https://esprima.readthedocs.io/en/latest/syntax-tree-format.html
   */

  /**
   * One of the rules of `grammar.pegjs`
   * @typedef {PlainObject} SelectorAST
   * @see grammar.pegjs
  */

  /**
   * The `sequence` production of `grammar.pegjs`
   * @typedef {PlainObject} SelectorSequenceAST
  */

  /**
   * Get the value of a property which may be multiple levels down
   * in the object.
   * @param {?PlainObject} obj
   * @param {string} key
   * @returns {undefined|boolean|string|number|external:AST}
   */

  function getPath(obj, key) {
    var keys = key.split('.');

    var _iterator = _createForOfIteratorHelper(keys),
        _step;

    try {
      for (_iterator.s(); !(_step = _iterator.n()).done;) {
        var _key = _step.value;

        if (obj == null) {
          return obj;
        }

        obj = obj[_key];
      }
    } catch (err) {
      _iterator.e(err);
    } finally {
      _iterator.f();
    }

    return obj;
  }
  /**
   * Determine whether `node` can be reached by following `path`,
   * starting at `ancestor`.
   * @param {?external:AST} node
   * @param {?external:AST} ancestor
   * @param {string[]} path
   * @returns {boolean}
   */


  function inPath(node, ancestor, path) {
    if (path.length === 0) {
      return node === ancestor;
    }

    if (ancestor == null) {
      return false;
    }

    var field = ancestor[path[0]];
    var remainingPath = path.slice(1);

    if (Array.isArray(field)) {
      var _iterator2 = _createForOfIteratorHelper(field),
          _step2;

      try {
        for (_iterator2.s(); !(_step2 = _iterator2.n()).done;) {
          var component = _step2.value;

          if (inPath(node, component, remainingPath)) {
            return true;
          }
        }
      } catch (err) {
        _iterator2.e(err);
      } finally {
        _iterator2.f();
      }

      return false;
    } else {
      return inPath(node, field, remainingPath);
    }
  }
  /**
   * @callback TraverseOptionFallback
   * @param {external:AST} node The given node.
   * @returns {string[]} An array of visitor keys for the given node.
   */

  /**
   * @typedef {object} ESQueryOptions
   * @property { { [nodeType: string]: string[] } } [visitorKeys] By passing `visitorKeys` mapping, we can extend the properties of the nodes that traverse the node.
   * @property {TraverseOptionFallback} [fallback] By passing `fallback` option, we can control the properties of traversing nodes when encountering unknown nodes.
   */

  /**
   * Given a `node` and its ancestors, determine if `node` is matched
   * by `selector`.
   * @param {?external:AST} node
   * @param {?SelectorAST} selector
   * @param {external:AST[]} [ancestry=[]]
   * @param {ESQueryOptions} [options]
   * @throws {Error} Unknowns (operator, class name, selector type, or
   * selector value type)
   * @returns {boolean}
   */


  function matches(node, selector, ancestry, options) {
    if (!selector) {
      return true;
    }

    if (!node) {
      return false;
    }

    if (!ancestry) {
      ancestry = [];
    }

    switch (selector.type) {
      case 'wildcard':
        return true;

      case 'identifier':
        return selector.value.toLowerCase() === node.type.toLowerCase();

      case 'field':
        {
          var path = selector.name.split('.');
          var ancestor = ancestry[path.length - 1];
          return inPath(node, ancestor, path);
        }

      case 'matches':
        var _iterator3 = _createForOfIteratorHelper(selector.selectors),
            _step3;

        try {
          for (_iterator3.s(); !(_step3 = _iterator3.n()).done;) {
            var sel = _step3.value;

            if (matches(node, sel, ancestry, options)) {
              return true;
            }
          }
        } catch (err) {
          _iterator3.e(err);
        } finally {
          _iterator3.f();
        }

        return false;

      case 'compound':
        var _iterator4 = _createForOfIteratorHelper(selector.selectors),
            _step4;

        try {
          for (_iterator4.s(); !(_step4 = _iterator4.n()).done;) {
            var _sel = _step4.value;

            if (!matches(node, _sel, ancestry, options)) {
              return false;
            }
          }
        } catch (err) {
          _iterator4.e(err);
        } finally {
          _iterator4.f();
        }

        return true;

      case 'not':
        var _iterator5 = _createForOfIteratorHelper(selector.selectors),
            _step5;

        try {
          for (_iterator5.s(); !(_step5 = _iterator5.n()).done;) {
            var _sel2 = _step5.value;

            if (matches(node, _sel2, ancestry, options)) {
              return false;
            }
          }
        } catch (err) {
          _iterator5.e(err);
        } finally {
          _iterator5.f();
        }

        return true;

      case 'has':
        {
          var _ret = function () {
            var collector = [];

            var _iterator6 = _createForOfIteratorHelper(selector.selectors),
                _step6;

            try {
              var _loop = function _loop() {
                var sel = _step6.value;
                var a = [];
                estraverse.traverse(node, {
                  enter: function enter(node, parent) {
                    if (parent != null) {
                      a.unshift(parent);
                    }

                    if (matches(node, sel, a, options)) {
                      collector.push(node);
                    }
                  },
                  leave: function leave() {
                    a.shift();
                  },
                  keys: options && options.visitorKeys,
                  fallback: options && options.fallback || 'iteration'
                });
              };

              for (_iterator6.s(); !(_step6 = _iterator6.n()).done;) {
                _loop();
              }
            } catch (err) {
              _iterator6.e(err);
            } finally {
              _iterator6.f();
            }

            return {
              v: collector.length !== 0
            };
          }();

          if (_typeof(_ret) === "object") return _ret.v;
        }

      case 'child':
        if (matches(node, selector.right, ancestry, options)) {
          return matches(ancestry[0], selector.left, ancestry.slice(1), options);
        }

        return false;

      case 'descendant':
        if (matches(node, selector.right, ancestry, options)) {
          for (var i = 0, l = ancestry.length; i < l; ++i) {
            if (matches(ancestry[i], selector.left, ancestry.slice(i + 1), options)) {
              return true;
            }
          }
        }

        return false;

      case 'attribute':
        {
          var p = getPath(node, selector.name);

          switch (selector.operator) {
            case void 0:
              return p != null;

            case '=':
              switch (selector.value.type) {
                case 'regexp':
                  return typeof p === 'string' && selector.value.value.test(p);

                case 'literal':
                  return "".concat(selector.value.value) === "".concat(p);

                case 'type':
                  return selector.value.value === _typeof(p);
              }

              throw new Error("Unknown selector value type: ".concat(selector.value.type));

            case '!=':
              switch (selector.value.type) {
                case 'regexp':
                  return !selector.value.value.test(p);

                case 'literal':
                  return "".concat(selector.value.value) !== "".concat(p);

                case 'type':
                  return selector.value.value !== _typeof(p);
              }

              throw new Error("Unknown selector value type: ".concat(selector.value.type));

            case '<=':
              return p <= selector.value.value;

            case '<':
              return p < selector.value.value;

            case '>':
              return p > selector.value.value;

            case '>=':
              return p >= selector.value.value;
          }

          throw new Error("Unknown operator: ".concat(selector.operator));
        }

      case 'sibling':
        return matches(node, selector.right, ancestry, options) && sibling(node, selector.left, ancestry, LEFT_SIDE, options) || selector.left.subject && matches(node, selector.left, ancestry, options) && sibling(node, selector.right, ancestry, RIGHT_SIDE, options);

      case 'adjacent':
        return matches(node, selector.right, ancestry, options) && adjacent(node, selector.left, ancestry, LEFT_SIDE, options) || selector.right.subject && matches(node, selector.left, ancestry, options) && adjacent(node, selector.right, ancestry, RIGHT_SIDE, options);

      case 'nth-child':
        return matches(node, selector.right, ancestry, options) && nthChild(node, ancestry, function () {
          return selector.index.value - 1;
        }, options);

      case 'nth-last-child':
        return matches(node, selector.right, ancestry, options) && nthChild(node, ancestry, function (length) {
          return length - selector.index.value;
        }, options);

      case 'class':
        switch (selector.name.toLowerCase()) {
          case 'statement':
            if (node.type.slice(-9) === 'Statement') return true;
          // fallthrough: interface Declaration <: Statement { }

          case 'declaration':
            return node.type.slice(-11) === 'Declaration';

          case 'pattern':
            if (node.type.slice(-7) === 'Pattern') return true;
          // fallthrough: interface Expression <: Node, Pattern { }

          case 'expression':
            return node.type.slice(-10) === 'Expression' || node.type.slice(-7) === 'Literal' || node.type === 'Identifier' && (ancestry.length === 0 || ancestry[0].type !== 'MetaProperty') || node.type === 'MetaProperty';

          case 'function':
            return node.type === 'FunctionDeclaration' || node.type === 'FunctionExpression' || node.type === 'ArrowFunctionExpression';
        }

        throw new Error("Unknown class name: ".concat(selector.name));
    }

    throw new Error("Unknown selector type: ".concat(selector.type));
  }
  /**
   * Get visitor keys of a given node.
   * @param {external:AST} node The AST node to get keys.
   * @param {ESQueryOptions|undefined} options
   * @returns {string[]} Visitor keys of the node.
   */


  function getVisitorKeys(node, options) {
    var nodeType = node.type;

    if (options && options.visitorKeys && options.visitorKeys[nodeType]) {
      return options.visitorKeys[nodeType];
    }

    if (estraverse.VisitorKeys[nodeType]) {
      return estraverse.VisitorKeys[nodeType];
    }

    if (options && typeof options.fallback === 'function') {
      return options.fallback(node);
    } // 'iteration' fallback


    return Object.keys(node).filter(function (key) {
      return key !== 'type';
    });
  }
  /**
   * Check whether the given value is an ASTNode or not.
   * @param {any} node The value to check.
   * @returns {boolean} `true` if the value is an ASTNode.
   */


  function isNode(node) {
    return node !== null && _typeof(node) === 'object' && typeof node.type === 'string';
  }
  /**
   * Determines if the given node has a sibling that matches the
   * given selector.
   * @param {external:AST} node
   * @param {SelectorSequenceAST} selector
   * @param {external:AST[]} ancestry
   * @param {Side} side
   * @param {ESQueryOptions|undefined} options
   * @returns {boolean}
   */


  function sibling(node, selector, ancestry, side, options) {
    var _ancestry = _slicedToArray(ancestry, 1),
        parent = _ancestry[0];

    if (!parent) {
      return false;
    }

    var keys = getVisitorKeys(parent, options);

    var _iterator7 = _createForOfIteratorHelper(keys),
        _step7;

    try {
      for (_iterator7.s(); !(_step7 = _iterator7.n()).done;) {
        var key = _step7.value;
        var listProp = parent[key];

        if (Array.isArray(listProp)) {
          var startIndex = listProp.indexOf(node);

          if (startIndex < 0) {
            continue;
          }

          var lowerBound = void 0,
              upperBound = void 0;

          if (side === LEFT_SIDE) {
            lowerBound = 0;
            upperBound = startIndex;
          } else {
            lowerBound = startIndex + 1;
            upperBound = listProp.length;
          }

          for (var k = lowerBound; k < upperBound; ++k) {
            if (isNode(listProp[k]) && matches(listProp[k], selector, ancestry, options)) {
              return true;
            }
          }
        }
      }
    } catch (err) {
      _iterator7.e(err);
    } finally {
      _iterator7.f();
    }

    return false;
  }
  /**
   * Determines if the given node has an adjacent sibling that matches
   * the given selector.
   * @param {external:AST} node
   * @param {SelectorSequenceAST} selector
   * @param {external:AST[]} ancestry
   * @param {Side} side
   * @param {ESQueryOptions|undefined} options
   * @returns {boolean}
   */


  function adjacent(node, selector, ancestry, side, options) {
    var _ancestry2 = _slicedToArray(ancestry, 1),
        parent = _ancestry2[0];

    if (!parent) {
      return false;
    }

    var keys = getVisitorKeys(parent, options);

    var _iterator8 = _createForOfIteratorHelper(keys),
        _step8;

    try {
      for (_iterator8.s(); !(_step8 = _iterator8.n()).done;) {
        var key = _step8.value;
        var listProp = parent[key];

        if (Array.isArray(listProp)) {
          var idx = listProp.indexOf(node);

          if (idx < 0) {
            continue;
          }

          if (side === LEFT_SIDE && idx > 0 && isNode(listProp[idx - 1]) && matches(listProp[idx - 1], selector, ancestry, options)) {
            return true;
          }

          if (side === RIGHT_SIDE && idx < listProp.length - 1 && isNode(listProp[idx + 1]) && matches(listProp[idx + 1], selector, ancestry, options)) {
            return true;
          }
        }
      }
    } catch (err) {
      _iterator8.e(err);
    } finally {
      _iterator8.f();
    }

    return false;
  }
  /**
  * @callback IndexFunction
  * @param {Integer} len Containing list's length
  * @returns {Integer}
  */

  /**
   * Determines if the given node is the nth child, determined by
   * `idxFn`, which is given the containing list's length.
   * @param {external:AST} node
   * @param {external:AST[]} ancestry
   * @param {IndexFunction} idxFn
   * @param {ESQueryOptions|undefined} options
   * @returns {boolean}
   */


  function nthChild(node, ancestry, idxFn, options) {
    var _ancestry3 = _slicedToArray(ancestry, 1),
        parent = _ancestry3[0];

    if (!parent) {
      return false;
    }

    var keys = getVisitorKeys(parent, options);

    var _iterator9 = _createForOfIteratorHelper(keys),
        _step9;

    try {
      for (_iterator9.s(); !(_step9 = _iterator9.n()).done;) {
        var key = _step9.value;
        var listProp = parent[key];

        if (Array.isArray(listProp)) {
          var idx = listProp.indexOf(node);

          if (idx >= 0 && idx === idxFn(listProp.length)) {
            return true;
          }
        }
      }
    } catch (err) {
      _iterator9.e(err);
    } finally {
      _iterator9.f();
    }

    return false;
  }
  /**
   * For each selector node marked as a subject, find the portion of the
   * selector that the subject must match.
   * @param {SelectorAST} selector
   * @param {SelectorAST} [ancestor] Defaults to `selector`
   * @returns {SelectorAST[]}
   */


  function subjects(selector, ancestor) {
    if (selector == null || _typeof(selector) != 'object') {
      return [];
    }

    if (ancestor == null) {
      ancestor = selector;
    }

    var results = selector.subject ? [ancestor] : [];

    for (var _i = 0, _Object$entries = _objectEntries(selector); _i < _Object$entries.length; _i++) {
      var _Object$entries$_i = _slicedToArray(_Object$entries[_i], 2),
          p = _Object$entries$_i[0],
          sel = _Object$entries$_i[1];

      results.push.apply(results, _toConsumableArray(subjects(sel, p === 'left' ? sel : ancestor)));
    }

    return results;
  }
  /**
  * @callback TraverseVisitor
  * @param {?external:AST} node
  * @param {?external:AST} parent
  * @param {external:AST[]} ancestry
  */

  /**
   * From a JS AST and a selector AST, collect all JS AST nodes that
   * match the selector.
   * @param {external:AST} ast
   * @param {?SelectorAST} selector
   * @param {TraverseVisitor} visitor
   * @param {ESQueryOptions} [options]
   * @returns {external:AST[]}
   */


  function traverse(ast, selector, visitor, options) {
    if (!selector) {
      return;
    }

    var ancestry = [];
    var altSubjects = subjects(selector);
    estraverse.traverse(ast, {
      enter: function enter(node, parent) {
        if (parent != null) {
          ancestry.unshift(parent);
        }

        if (matches(node, selector, ancestry, options)) {
          if (altSubjects.length) {
            for (var i = 0, l = altSubjects.length; i < l; ++i) {
              if (matches(node, altSubjects[i], ancestry, options)) {
                visitor(node, parent, ancestry);
              }

              for (var k = 0, m = ancestry.length; k < m; ++k) {
                var succeedingAncestry = ancestry.slice(k + 1);

                if (matches(ancestry[k], altSubjects[i], succeedingAncestry, options)) {
                  visitor(ancestry[k], parent, succeedingAncestry);
                }
              }
            }
          } else {
            visitor(node, parent, ancestry);
          }
        }
      },
      leave: function leave() {
        ancestry.shift();
      },
      keys: options && options.visitorKeys,
      fallback: options && options.fallback || 'iteration'
    });
  }
  /**
   * From a JS AST and a selector AST, collect all JS AST nodes that
   * match the selector.
   * @param {external:AST} ast
   * @param {?SelectorAST} selector
   * @param {ESQueryOptions} [options]
   * @returns {external:AST[]}
   */


  function match(ast, selector, options) {
    var results = [];
    traverse(ast, selector, function (node) {
      results.push(node);
    }, options);
    return results;
  }
  /**
   * Parse a selector string and return its AST.
   * @param {string} selector
   * @returns {SelectorAST}
   */


  function parse(selector) {
    return parser.parse(selector);
  }
  /**
   * Query the code AST using the selector string.
   * @param {external:AST} ast
   * @param {string} selector
   * @param {ESQueryOptions} [options]
   * @returns {external:AST[]}
   */


  function query(ast, selector, options) {
    return match(ast, parse(selector), options);
  }

  query.parse = parse;
  query.match = match;
  query.traverse = traverse;
  query.matches = matches;
  query.query = query;

  return query;

})));
