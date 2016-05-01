'use strict';

// Regexes used for ansi escape code splitting
// eslint-disable-next-line no-control-regex
const metaKeyCodeReAnywhere = /(?:\x1b)([a-zA-Z0-9])/;
const functionKeyCodeReAnywhere = new RegExp('(?:\x1b+)(O|N|\\[|\\[\\[)(?:' + [
  '(\\d+)(?:;(\\d+))?([~^$])',
  '(?:M([@ #!a`])(.)(.))', // mouse
  '(?:1;)?(\\d+)?([a-zA-Z])'
].join('|') + ')');


module.exports = {
  emitKeys,
  getStringWidth,
  isFullWidthCodePoint,
  stripVTControlCharacters
};


/**
 * Returns the number of columns required to display the given string.
 */
function getStringWidth(str) {
  let width = 0;

  str = stripVTControlCharacters(str);

  for (let i = 0; i < str.length; i++) {
    const code = str.codePointAt(i);

    if (code >= 0x10000) { // surrogates
      i++;
    }

    if (isFullWidthCodePoint(code)) {
      width += 2;
    } else {
      width++;
    }
  }

  return width;
}


/**
 * Returns true if the character represented by a given
 * Unicode code point is full-width. Otherwise returns false.
 */
function isFullWidthCodePoint(code) {
  if (isNaN(code)) {
    return false;
  }

  // Code points are derived from:
  // http://www.unicode.org/Public/UNIDATA/EastAsianWidth.txt
  if (code >= 0x1100 && (
      code <= 0x115f ||  // Hangul Jamo
      0x2329 === code || // LEFT-POINTING ANGLE BRACKET
      0x232a === code || // RIGHT-POINTING ANGLE BRACKET
      // CJK Radicals Supplement .. Enclosed CJK Letters and Months
      (0x2e80 <= code && code <= 0x3247 && code !== 0x303f) ||
      // Enclosed CJK Letters and Months .. CJK Unified Ideographs Extension A
      0x3250 <= code && code <= 0x4dbf ||
      // CJK Unified Ideographs .. Yi Radicals
      0x4e00 <= code && code <= 0xa4c6 ||
      // Hangul Jamo Extended-A
      0xa960 <= code && code <= 0xa97c ||
      // Hangul Syllables
      0xac00 <= code && code <= 0xd7a3 ||
      // CJK Compatibility Ideographs
      0xf900 <= code && code <= 0xfaff ||
      // Vertical Forms
      0xfe10 <= code && code <= 0xfe19 ||
      // CJK Compatibility Forms .. Small Form Variants
      0xfe30 <= code && code <= 0xfe6b ||
      // Halfwidth and Fullwidth Forms
      0xff01 <= code && code <= 0xff60 ||
      0xffe0 <= code && code <= 0xffe6 ||
      // Kana Supplement
      0x1b000 <= code && code <= 0x1b001 ||
      // Enclosed Ideographic Supplement
      0x1f200 <= code && code <= 0x1f251 ||
      // CJK Unified Ideographs Extension B .. Tertiary Ideographic Plane
      0x20000 <= code && code <= 0x3fffd)) {
    return true;
  }

  return false;
}


/**
 * Tries to remove all VT control characters. Use to estimate displayed
 * string width. May be buggy due to not running a real state machine
 */
function stripVTControlCharacters(str) {
  str = str.replace(new RegExp(functionKeyCodeReAnywhere.source, 'g'), '');
  return str.replace(new RegExp(metaKeyCodeReAnywhere.source, 'g'), '');
}


/*
  Some patterns seen in terminal key escape codes, derived from combos seen
  at http://www.midnight-commander.org/browser/lib/tty/key.c

  ESC letter
  ESC [ letter
  ESC [ modifier letter
  ESC [ 1 ; modifier letter
  ESC [ num char
  ESC [ num ; modifier char
  ESC O letter
  ESC O modifier letter
  ESC O 1 ; modifier letter
  ESC N letter
  ESC [ [ num ; modifier char
  ESC [ [ 1 ; modifier letter
  ESC ESC [ num char
  ESC ESC O letter

  - char is usually ~ but $ and ^ also happen with rxvt
  - modifier is 1 +
                (shift     * 1) +
                (left_alt  * 2) +
                (ctrl      * 4) +
                (right_alt * 8)
  - two leading ESCs apparently mean the same as one leading ESC
*/
function* emitKeys(stream) {
  while (true) {
    let ch = yield;
    let s = ch;
    let escaped = false;
    const key = {
      sequence: null,
      name: undefined,
      ctrl: false,
      meta: false,
      shift: false
    };

    if (ch === '\x1b') {
      escaped = true;
      s += (ch = yield);

      if (ch === '\x1b') {
        s += (ch = yield);
      }
    }

    if (escaped && (ch === 'O' || ch === '[')) {
      // ansi escape sequence
      let code = ch;
      let modifier = 0;

      if (ch === 'O') {
        // ESC O letter
        // ESC O modifier letter
        s += (ch = yield);

        if (ch >= '0' && ch <= '9') {
          modifier = (ch >> 0) - 1;
          s += (ch = yield);
        }

        code += ch;
      } else if (ch === '[') {
        // ESC [ letter
        // ESC [ modifier letter
        // ESC [ [ modifier letter
        // ESC [ [ num char
        s += (ch = yield);

        if (ch === '[') {
          // \x1b[[A
          //      ^--- escape codes might have a second bracket
          code += ch;
          s += (ch = yield);
        }

        /*
         * Here and later we try to buffer just enough data to get
         * a complete ascii sequence.
         *
         * We have basically two classes of ascii characters to process:
         *
         *
         * 1. `\x1b[24;5~` should be parsed as { code: '[24~', modifier: 5 }
         *
         * This particular example is featuring Ctrl+F12 in xterm.
         *
         *  - `;5` part is optional, e.g. it could be `\x1b[24~`
         *  - first part can contain one or two digits
         *
         * So the generic regexp is like /^\d\d?(;\d)?[~^$]$/
         *
         *
         * 2. `\x1b[1;5H` should be parsed as { code: '[H', modifier: 5 }
         *
         * This particular example is featuring Ctrl+Home in xterm.
         *
         *  - `1;5` part is optional, e.g. it could be `\x1b[H`
         *  - `1;` part is optional, e.g. it could be `\x1b[5H`
         *
         * So the generic regexp is like /^((\d;)?\d)?[A-Za-z]$/
         *
         */
        const cmdStart = s.length - 1;

        // skip one or two leading digits
        if (ch >= '0' && ch <= '9') {
          s += (ch = yield);

          if (ch >= '0' && ch <= '9') {
            s += (ch = yield);
          }
        }

        // skip modifier
        if (ch === ';') {
          s += (ch = yield);

          if (ch >= '0' && ch <= '9') {
            s += (ch = yield);
          }
        }

        /*
         * We buffered enough data, now trying to extract code
         * and modifier from it
         */
        const cmd = s.slice(cmdStart);
        let match;

        if ((match = cmd.match(/^(\d\d?)(;(\d))?([~^$])$/))) {
          code += match[1] + match[4];
          modifier = (match[3] || 1) - 1;
        } else if ((match = cmd.match(/^((\d;)?(\d))?([A-Za-z])$/))) {
          code += match[4];
          modifier = (match[3] || 1) - 1;
        } else {
          code += cmd;
        }
      }

      // Parse the key modifier
      key.ctrl = !!(modifier & 4);
      key.meta = !!(modifier & 10);
      key.shift = !!(modifier & 1);
      key.code = code;

      // Parse the key itself
      switch (code) {
        /* xterm/gnome ESC O letter */
        case 'OP': key.name = 'f1'; break;
        case 'OQ': key.name = 'f2'; break;
        case 'OR': key.name = 'f3'; break;
        case 'OS': key.name = 'f4'; break;

        /* xterm/rxvt ESC [ number ~ */
        case '[11~': key.name = 'f1'; break;
        case '[12~': key.name = 'f2'; break;
        case '[13~': key.name = 'f3'; break;
        case '[14~': key.name = 'f4'; break;

        /* from Cygwin and used in libuv */
        case '[[A': key.name = 'f1'; break;
        case '[[B': key.name = 'f2'; break;
        case '[[C': key.name = 'f3'; break;
        case '[[D': key.name = 'f4'; break;
        case '[[E': key.name = 'f5'; break;

        /* common */
        case '[15~': key.name = 'f5'; break;
        case '[17~': key.name = 'f6'; break;
        case '[18~': key.name = 'f7'; break;
        case '[19~': key.name = 'f8'; break;
        case '[20~': key.name = 'f9'; break;
        case '[21~': key.name = 'f10'; break;
        case '[23~': key.name = 'f11'; break;
        case '[24~': key.name = 'f12'; break;

        /* xterm ESC [ letter */
        case '[A': key.name = 'up'; break;
        case '[B': key.name = 'down'; break;
        case '[C': key.name = 'right'; break;
        case '[D': key.name = 'left'; break;
        case '[E': key.name = 'clear'; break;
        case '[F': key.name = 'end'; break;
        case '[H': key.name = 'home'; break;

        /* xterm/gnome ESC O letter */
        case 'OA': key.name = 'up'; break;
        case 'OB': key.name = 'down'; break;
        case 'OC': key.name = 'right'; break;
        case 'OD': key.name = 'left'; break;
        case 'OE': key.name = 'clear'; break;
        case 'OF': key.name = 'end'; break;
        case 'OH': key.name = 'home'; break;

        /* xterm/rxvt ESC [ number ~ */
        case '[1~': key.name = 'home'; break;
        case '[2~': key.name = 'insert'; break;
        case '[3~': key.name = 'delete'; break;
        case '[4~': key.name = 'end'; break;
        case '[5~': key.name = 'pageup'; break;
        case '[6~': key.name = 'pagedown'; break;

        /* putty */
        case '[[5~': key.name = 'pageup'; break;
        case '[[6~': key.name = 'pagedown'; break;

        /* rxvt */
        case '[7~': key.name = 'home'; break;
        case '[8~': key.name = 'end'; break;

        /* rxvt keys with modifiers */
        case '[a': key.name = 'up'; key.shift = true; break;
        case '[b': key.name = 'down'; key.shift = true; break;
        case '[c': key.name = 'right'; key.shift = true; break;
        case '[d': key.name = 'left'; key.shift = true; break;
        case '[e': key.name = 'clear'; key.shift = true; break;

        case '[2$': key.name = 'insert'; key.shift = true; break;
        case '[3$': key.name = 'delete'; key.shift = true; break;
        case '[5$': key.name = 'pageup'; key.shift = true; break;
        case '[6$': key.name = 'pagedown'; key.shift = true; break;
        case '[7$': key.name = 'home'; key.shift = true; break;
        case '[8$': key.name = 'end'; key.shift = true; break;

        case 'Oa': key.name = 'up'; key.ctrl = true; break;
        case 'Ob': key.name = 'down'; key.ctrl = true; break;
        case 'Oc': key.name = 'right'; key.ctrl = true; break;
        case 'Od': key.name = 'left'; key.ctrl = true; break;
        case 'Oe': key.name = 'clear'; key.ctrl = true; break;

        case '[2^': key.name = 'insert'; key.ctrl = true; break;
        case '[3^': key.name = 'delete'; key.ctrl = true; break;
        case '[5^': key.name = 'pageup'; key.ctrl = true; break;
        case '[6^': key.name = 'pagedown'; key.ctrl = true; break;
        case '[7^': key.name = 'home'; key.ctrl = true; break;
        case '[8^': key.name = 'end'; key.ctrl = true; break;

        /* misc. */
        case '[Z': key.name = 'tab'; key.shift = true; break;
        default: key.name = 'undefined'; break;
      }
    } else if (ch === '\r') {
      // carriage return
      key.name = 'return';
    } else if (ch === '\n') {
      // enter, should have been called linefeed
      key.name = 'enter';
    } else if (ch === '\t') {
      // tab
      key.name = 'tab';
    } else if (ch === '\b' || ch === '\x7f') {
      // backspace or ctrl+h
      key.name = 'backspace';
      key.meta = escaped;
    } else if (ch === '\x1b') {
      // escape key
      key.name = 'escape';
      key.meta = escaped;
    } else if (ch === ' ') {
      key.name = 'space';
      key.meta = escaped;
    } else if (!escaped && ch <= '\x1a') {
      // ctrl+letter
      key.name = String.fromCharCode(ch.charCodeAt(0) + 'a'.charCodeAt(0) - 1);
      key.ctrl = true;
    } else if (/^[0-9A-Za-z]$/.test(ch)) {
      // letter, number, shift+letter
      key.name = ch.toLowerCase();
      key.shift = /^[A-Z]$/.test(ch);
      key.meta = escaped;
    }

    key.sequence = s;

    if (key.name !== undefined) {
      /* Named character or sequence */
      stream.emit('keypress', escaped ? undefined : s, key);
    } else if (s.length === 1) {
      /* Single unnamed character, e.g. "." */
      stream.emit('keypress', s, key);
    }
    /* Unrecognized or broken escape sequence, don't emit anything */
  }
}
