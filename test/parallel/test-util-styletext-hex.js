'use strict';

const common = require('../common');
const assert = require('node:assert');
const { describe, it } = require('node:test');
const util = require('node:util');
const { WriteStream } = require('node:tty');

describe('util.styleText hex color support', () => {
  describe('valid 6-digit hex colors', () => {
    it('should parse #ffcc00 as RGB(255, 204, 0)', () => {
      const styled = util.styleText('#ffcc00', 'test', { validateStream: false });
      assert.strictEqual(styled, '\u001b[38;2;255;204;0mtest\u001b[39m');
    });

    it('should parse #000000 as RGB(0, 0, 0) - black', () => {
      const styled = util.styleText('#000000', 'test', { validateStream: false });
      assert.strictEqual(styled, '\u001b[38;2;0;0;0mtest\u001b[39m');
    });

    it('should parse #ffffff as RGB(255, 255, 255) - white', () => {
      const styled = util.styleText('#ffffff', 'test', { validateStream: false });
      assert.strictEqual(styled, '\u001b[38;2;255;255;255mtest\u001b[39m');
    });

    it('should parse uppercase #AABBCC as RGB(170, 187, 204)', () => {
      const styled = util.styleText('#AABBCC', 'test', { validateStream: false });
      assert.strictEqual(styled, '\u001b[38;2;170;187;204mtest\u001b[39m');
    });

    it('should parse mixed case #aAbBcC as RGB(170, 187, 204)', () => {
      const styled = util.styleText('#aAbBcC', 'test', { validateStream: false });
      assert.strictEqual(styled, '\u001b[38;2;170;187;204mtest\u001b[39m');
    });
  });

  describe('valid 3-digit hex colors (shorthand)', () => {
    it('should expand #fc0 to #ffcc00 -> RGB(255, 204, 0)', () => {
      const styled = util.styleText('#fc0', 'test', { validateStream: false });
      assert.strictEqual(styled, '\u001b[38;2;255;204;0mtest\u001b[39m');
    });

    it('should parse #000 as RGB(0, 0, 0)', () => {
      const styled = util.styleText('#000', 'test', { validateStream: false });
      assert.strictEqual(styled, '\u001b[38;2;0;0;0mtest\u001b[39m');
    });

    it('should parse #fff as RGB(255, 255, 255)', () => {
      const styled = util.styleText('#fff', 'test', { validateStream: false });
      assert.strictEqual(styled, '\u001b[38;2;255;255;255mtest\u001b[39m');
    });

    it('should parse uppercase #FFF as RGB(255, 255, 255)', () => {
      const styled = util.styleText('#FFF', 'test', { validateStream: false });
      assert.strictEqual(styled, '\u001b[38;2;255;255;255mtest\u001b[39m');
    });

    it('should expand #abc to #aabbcc -> RGB(170, 187, 204)', () => {
      const styled = util.styleText('#abc', 'test', { validateStream: false });
      assert.strictEqual(styled, '\u001b[38;2;170;187;204mtest\u001b[39m');
    });
  });

  describe('combining hex colors with other formats', () => {
    it('should combine bold and hex color', () => {
      const styled = util.styleText(['bold', '#ff0000'], 'test', { validateStream: false });
      assert.strictEqual(styled, '\u001b[1m\u001b[38;2;255;0;0mtest\u001b[39m\u001b[22m');
    });

    it('should combine hex color and underline', () => {
      const styled = util.styleText(['#00ff00', 'underline'], 'test', { validateStream: false });
      assert.strictEqual(styled, '\u001b[38;2;0;255;0m\u001b[4mtest\u001b[24m\u001b[39m');
    });

    it('should handle none format with hex color', () => {
      const styled = util.styleText(['none', '#ff0000'], 'test', { validateStream: false });
      assert.strictEqual(styled, '\u001b[38;2;255;0;0mtest\u001b[39m');
    });
  });

  describe('invalid hex strings', () => {
    it('should throw for missing # prefix', () => {
      assert.throws(() => {
        util.styleText('ffcc00', 'test', { validateStream: false });
      }, {
        code: 'ERR_INVALID_ARG_VALUE',
      });
    });

    it('should throw for invalid characters', () => {
      assert.throws(() => {
        util.styleText('#gggggg', 'test', { validateStream: false });
      }, {
        code: 'ERR_INVALID_ARG_VALUE',
        message: /must be a valid hex color/,
      });
    });

    it('should throw for wrong length (4 digits)', () => {
      assert.throws(() => {
        util.styleText('#ffcc', 'test', { validateStream: false });
      }, {
        code: 'ERR_INVALID_ARG_VALUE',
        message: /must be a valid hex color/,
      });
    });

    it('should throw for wrong length (5 digits)', () => {
      assert.throws(() => {
        util.styleText('#ffcc0', 'test', { validateStream: false });
      }, {
        code: 'ERR_INVALID_ARG_VALUE',
        message: /must be a valid hex color/,
      });
    });

    it('should throw for wrong length (7 digits)', () => {
      assert.throws(() => {
        util.styleText('#ffcc000', 'test', { validateStream: false });
      }, {
        code: 'ERR_INVALID_ARG_VALUE',
        message: /must be a valid hex color/,
      });
    });

    it('should throw for empty after #', () => {
      assert.throws(() => {
        util.styleText('#', 'test', { validateStream: false });
      }, {
        code: 'ERR_INVALID_ARG_VALUE',
        message: /must be a valid hex color/,
      });
    });

    it('should throw for invalid hex in array', () => {
      assert.throws(() => {
        util.styleText(['bold', '#xyz'], 'test', { validateStream: false });
      }, {
        code: 'ERR_INVALID_ARG_VALUE',
        message: /must be a valid hex color/,
      });
    });
  });

  describe('environment variable behavior', () => {
    const styledHex = '\u001b[38;2;255;204;0mtest\u001b[39m';
    const noChange = 'test';

    const fd = common.getTTYfd();
    if (fd === -1) {
      it.skip('Could not create TTY fd', () => {});
    } else {
      const writeStream = new WriteStream(fd);
      const originalEnv = { ...process.env };

      const testCases = [
        {
          isTTY: true,
          env: {},
          expected: styledHex,
          description: 'isTTY=true with no env vars',
        },
        {
          isTTY: false,
          env: {},
          expected: noChange,
          description: 'isTTY=false with no env vars',
        },
        {
          isTTY: true,
          env: { NODE_DISABLE_COLORS: '1' },
          expected: noChange,
          description: 'NODE_DISABLE_COLORS=1',
        },
        {
          isTTY: true,
          env: { NO_COLOR: '1' },
          expected: noChange,
          description: 'NO_COLOR=1',
        },
        {
          isTTY: true,
          env: { FORCE_COLOR: '1' },
          expected: styledHex,
          description: 'FORCE_COLOR=1',
        },
        {
          isTTY: true,
          env: { FORCE_COLOR: '1', NODE_DISABLE_COLORS: '1' },
          expected: styledHex,
          description: 'FORCE_COLOR=1 overrides NODE_DISABLE_COLORS',
        },
        {
          isTTY: false,
          env: { FORCE_COLOR: '1', NO_COLOR: '1', NODE_DISABLE_COLORS: '1' },
          expected: styledHex,
          description: 'FORCE_COLOR=1 overrides all disable flags',
        },
        {
          isTTY: true,
          env: { FORCE_COLOR: '1', NO_COLOR: '1', NODE_DISABLE_COLORS: '1' },
          expected: styledHex,
          description: 'FORCE_COLOR=1 wins with all flags',
        },
        {
          isTTY: true,
          env: { FORCE_COLOR: '0' },
          expected: noChange,
          description: 'FORCE_COLOR=0 disables colors',
        },
      ];

      for (const testCase of testCases) {
        it(`should respect ${testCase.description}`, () => {
          writeStream.isTTY = testCase.isTTY;
          process.env = {
            ...originalEnv,
            ...testCase.env,
          };
          const output = util.styleText('#ffcc00', 'test', { stream: writeStream });
          assert.strictEqual(output, testCase.expected);
          process.env = originalEnv;
        });
      }
    }
  });

  describe('nested hex colors', () => {
    it('should handle nested hex color styling', () => {
      const inner = util.styleText('#0000ff', 'inner', { validateStream: false });
      const outer = util.styleText('#ff0000', `before${inner}after`, { validateStream: false });
      assert.strictEqual(
        outer,
        '\u001b[38;2;255;0;0mbefore\u001b[38;2;0;0;255minner\u001b[38;2;255;0;0mafter\u001b[39m'
      );
    });
  });

  describe('multiple hex colors in array', () => {
    it('should apply multiple hex colors in order', () => {
      const styled = util.styleText(['#ff0000', '#00ff00'], 'test', { validateStream: false });
      assert.strictEqual(
        styled,
        '\u001b[38;2;255;0;0m\u001b[38;2;0;255;0mtest\u001b[39m\u001b[39m'
      );
    });
  });
});
