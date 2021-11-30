/* eslint-disable node/no-unsupported-features/es-syntax */
// We copy the regular expression so as to be able to always
// ensure the exec expression is a global one (and thereby prevent recursion)

/**
 *
 * @param {RegExtras} RegExtras
 * @returns {void}
 */
function addPrototypeMethods(RegExtras) {
  RegExtras.prototype.entries = /*#__PURE__*/regeneratorRuntime.mark(function _callee(str) {
    var matches, i, regex;
    return regeneratorRuntime.wrap(function _callee$(_context) {
      while (1) {
        switch (_context.prev = _context.next) {
          case 0:
            i = 0;
            regex = RegExtras.mixinRegex(this.regex, 'g');

          case 2:
            if (!((matches = regex.exec(str)) !== null)) {
              _context.next = 7;
              break;
            }

            _context.next = 5;
            return [i++, matches];

          case 5:
            _context.next = 2;
            break;

          case 7:
          case "end":
            return _context.stop();
        }
      }
    }, _callee, this);
  });
  RegExtras.prototype.values = /*#__PURE__*/regeneratorRuntime.mark(function _callee2(str) {
    var matches, regex;
    return regeneratorRuntime.wrap(function _callee2$(_context2) {
      while (1) {
        switch (_context2.prev = _context2.next) {
          case 0:
            regex = RegExtras.mixinRegex(this.regex, 'g');

          case 1:
            if (!((matches = regex.exec(str)) !== null)) {
              _context2.next = 6;
              break;
            }

            _context2.next = 4;
            return matches;

          case 4:
            _context2.next = 1;
            break;

          case 6:
          case "end":
            return _context2.stop();
        }
      }
    }, _callee2, this);
  });
  RegExtras.prototype.keys = /*#__PURE__*/regeneratorRuntime.mark(function _callee3(str) {
    var i, regex;
    return regeneratorRuntime.wrap(function _callee3$(_context3) {
      while (1) {
        switch (_context3.prev = _context3.next) {
          case 0:
            i = 0;
            regex = RegExtras.mixinRegex(this.regex, 'g');

          case 2:
            if (!(regex.exec(str) !== null)) {
              _context3.next = 7;
              break;
            }

            _context3.next = 5;
            return i++;

          case 5:
            _context3.next = 2;
            break;

          case 7:
          case "end":
            return _context3.stop();
        }
      }
    }, _callee3, this);
  });
}

export default addPrototypeMethods;
