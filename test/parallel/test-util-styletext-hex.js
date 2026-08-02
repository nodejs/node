'use strict';

const common = require('../common');
const assert = require('node:assert');
const { describe, it } = require('node:test');
const util = require('node:util');
const { WriteStream } = require('node:tty');

// Hex colors are downgraded to the color depth reported by `FORCE_COLOR`, so
// make sure the environment running the test does not set it.
delete process.env.FORCE_COLOR;

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
    // #ffcc00 in each of the supported color depths.
    const styledHex = '\u001b[38;2;255;204;0mtest\u001b[39m';
    const styledHex256 = '\u001b[38;5;220mtest\u001b[39m';
    const styledHex16 = '\u001b[93mtest\u001b[39m';
    const noChange = 'test';

    // The output expected from a terminal supporting `depth` bits of color.
    function styledForDepth(depth) {
      if (depth >= 24) return styledHex;
      if (depth >= 8) return styledHex256;
      return styledHex16;
    }

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
          // Depends on the color depth of the terminal running the test.
          expected: () => styledForDepth(writeStream.getColorDepth()),
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
          expected: styledHex16,
          description: 'FORCE_COLOR=1 downgrading to 16 colors',
        },
        {
          isTTY: true,
          env: { FORCE_COLOR: 'true' },
          expected: styledHex16,
          description: 'FORCE_COLOR=true downgrading to 16 colors',
        },
        {
          isTTY: true,
          env: { FORCE_COLOR: '' },
          expected: styledHex16,
          description: 'an empty FORCE_COLOR downgrading to 16 colors',
        },
        {
          isTTY: true,
          env: { FORCE_COLOR: '2' },
          expected: styledHex256,
          description: 'FORCE_COLOR=2 downgrading to 256 colors',
        },
        {
          isTTY: true,
          env: { FORCE_COLOR: '3' },
          expected: styledHex,
          description: 'FORCE_COLOR=3 keeping 24-bit colors',
        },
        {
          isTTY: false,
          env: { FORCE_COLOR: '3' },
          expected: styledHex,
          description: 'FORCE_COLOR=3 with isTTY=false',
        },
        {
          isTTY: true,
          env: { FORCE_COLOR: '1', NODE_DISABLE_COLORS: '1' },
          expected: styledHex16,
          description: 'FORCE_COLOR=1 overrides NODE_DISABLE_COLORS',
        },
        {
          isTTY: false,
          env: { FORCE_COLOR: '2', NO_COLOR: '1', NODE_DISABLE_COLORS: '1' },
          expected: styledHex256,
          description: 'FORCE_COLOR=2 overrides all disable flags',
        },
        {
          isTTY: true,
          env: { FORCE_COLOR: '3', NO_COLOR: '1', NODE_DISABLE_COLORS: '1' },
          expected: styledHex,
          description: 'FORCE_COLOR=3 wins with all flags',
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
          const expected = typeof testCase.expected === 'function' ?
            testCase.expected() :
            testCase.expected;
          const output = util.styleText('#ffcc00', 'test', { stream: writeStream });
          assert.strictEqual(output, expected);
          // Combining the hex color with another format applies the same depth.
          const combined = util.styleText(['bold', '#ffcc00'], 'test', { stream: writeStream });
          assert.strictEqual(
            combined,
            expected === noChange ? noChange : `\u001b[1m${expected}\u001b[22m`,
          );
          process.env = originalEnv;
        });
      }
    }
  });

  describe('color depth downgrade without stream validation', () => {
    const originalEnv = { ...process.env };

    function styled(format, forceColor) {
      process.env = { ...originalEnv };
      if (forceColor === undefined) {
        delete process.env.FORCE_COLOR;
      } else {
        process.env.FORCE_COLOR = forceColor;
      }
      try {
        return util.styleText(format, 'test', { validateStream: false });
      } finally {
        process.env = originalEnv;
      }
    }

    it('should keep 24-bit colors when FORCE_COLOR is not set', () => {
      assert.strictEqual(styled('#ffcc00'), '\u001b[38;2;255;204;0mtest\u001b[39m');
    });

    it('should downgrade to 16 colors with FORCE_COLOR=1', () => {
      assert.strictEqual(styled('#ffcc00', '1'), '\u001b[93mtest\u001b[39m');
    });

    it('should downgrade to 256 colors with FORCE_COLOR=2', () => {
      assert.strictEqual(styled('#ffcc00', '2'), '\u001b[38;5;220mtest\u001b[39m');
    });

    it('should keep 24-bit colors with FORCE_COLOR=3', () => {
      assert.strictEqual(styled('#ffcc00', '3'), '\u001b[38;2;255;204;0mtest\u001b[39m');
    });

    it('should keep 24-bit colors with FORCE_COLOR=0', () => {
      // Colors are not disabled when the stream is not validated.
      assert.strictEqual(styled('#ffcc00', '0'), '\u001b[38;2;255;204;0mtest\u001b[39m');
    });

    it('should downgrade every color of an array of formats', () => {
      assert.strictEqual(
        styled(['#ff0000', 'underline', '#00ff00'], '2'),
        '\u001b[38;5;196m\u001b[4m\u001b[38;5;46mtest\u001b[39m\u001b[24m\u001b[39m',
      );
    });
  });

  describe('closest color for each depth', () => {
    const originalEnv = { ...process.env };

    function styled(format, forceColor) {
      process.env = { ...originalEnv, FORCE_COLOR: forceColor };
      try {
        return util.styleText(format, 'x', { validateStream: false });
      } finally {
        process.env = originalEnv;
      }
    }

    it('should map colors to the closest of the 16 basic colors', () => {
      assert.strictEqual(styled('#000000', '1'), '\u001b[30mx\u001b[39m');
      assert.strictEqual(styled('#ff0000', '1'), '\u001b[91mx\u001b[39m');
      assert.strictEqual(styled('#00ff00', '1'), '\u001b[92mx\u001b[39m');
      assert.strictEqual(styled('#0000ff', '1'), '\u001b[94mx\u001b[39m');
      assert.strictEqual(styled('#00ffff', '1'), '\u001b[96mx\u001b[39m');
      assert.strictEqual(styled('#ff00ff', '1'), '\u001b[95mx\u001b[39m');
      assert.strictEqual(styled('#ffffff', '1'), '\u001b[97mx\u001b[39m');
      assert.strictEqual(styled('#808080', '1'), '\u001b[37mx\u001b[39m');
      // Only a fully saturated channel switches to the bright variant, so
      // mid-tones keep the normal colors.
      assert.strictEqual(styled('#aabbcc', '1'), '\u001b[37mx\u001b[39m');
      assert.strictEqual(styled('#f0f0f0', '1'), '\u001b[37mx\u001b[39m');
      assert.strictEqual(styled('#ffcc00', '1'), '\u001b[93mx\u001b[39m');
      // Colors too dark to be told apart end up black.
      assert.strictEqual(styled('#123456', '1'), '\u001b[30mx\u001b[39m');
    });

    it('should map colors to the closest of the 256 color palette', () => {
      // Both ends of the 6x6x6 color cube.
      assert.strictEqual(styled('#000000', '2'), '\u001b[38;5;16mx\u001b[39m');
      assert.strictEqual(styled('#ffffff', '2'), '\u001b[38;5;231mx\u001b[39m');
      assert.strictEqual(styled('#ff0000', '2'), '\u001b[38;5;196mx\u001b[39m');
      assert.strictEqual(styled('#00ff00', '2'), '\u001b[38;5;46mx\u001b[39m');
      assert.strictEqual(styled('#0000ff', '2'), '\u001b[38;5;21mx\u001b[39m');
      // Grayscale ramp.
      assert.strictEqual(styled('#080808', '2'), '\u001b[38;5;232mx\u001b[39m');
      assert.strictEqual(styled('#808080', '2'), '\u001b[38;5;244mx\u001b[39m');
      assert.strictEqual(styled('#f8f8f8', '2'), '\u001b[38;5;255mx\u001b[39m');
    });
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
