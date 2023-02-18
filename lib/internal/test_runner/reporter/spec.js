'use strict';

const {
  ArrayPrototypeJoin,
  ArrayPrototypePop,
  ArrayPrototypeShift,
  ArrayPrototypeUnshift,
  hardenRegExp,
  RegExpPrototypeSymbolSplit,
  SafeMap,
  StringPrototypeRepeat,
} = primordials;
const assert = require('assert');
const Transform = require('internal/streams/transform');
const { inspectWithNoCustomRetry } = require('internal/errors');
const { green, blue, red, white, gray } = require('internal/util/colors');


const inspectOptions = { __proto__: null, colors: true, breakLength: Infinity };

const colors = {
  '__proto__': null,
  'test:fail': red,
  'test:pass': green,
  'test:diagnostic': blue,
};
const symbols = {
  '__proto__': null,
  'test:fail': '\u2716 ',
  'test:pass': '\u2714 ',
  'test:diagnostic': '\u2139 ',
  'arrow:right': '\u25B6 ',
  'hyphen:minus': '\uFE63 ',
};
class SpecReporter extends Transform {
  #stack = [];
  #reported = [];
  #indentMemo = new SafeMap();

  constructor() {
    super({ writableObjectMode: true });
  }

  #indent(nesting) {
    let value = this.#indentMemo.get(nesting);
    if (value === undefined) {
      value = StringPrototypeRepeat('  ', nesting);
      this.#indentMemo.set(nesting, value);
    }

    return value;
  }
  #formatError(error, indent) {
    if (!error) return '';
    const err = error.code === 'ERR_TEST_FAILURE' ? error.cause : error;
    const message = ArrayPrototypeJoin(
      RegExpPrototypeSymbolSplit(
        hardenRegExp(/\r?\n/),
        inspectWithNoCustomRetry(err, inspectOptions),
      ), `\n${indent}  `);
    return `\n${indent}  ${message}\n`;
  }
  #handleEvent({ type, data }) {
    let color = colors[type] ?? white;
    let symbol = symbols[type] ?? ' ';

    switch (type) {
      case 'test:fail':
      case 'test:pass': {
        const subtest = ArrayPrototypeShift(this.#stack); // This is the matching `test:start` event
        if (subtest) {
          assert(subtest.type === 'test:start');
          assert(subtest.data.nesting === data.nesting);
          assert(subtest.data.name === data.name);
        }
        let prefix = '';
        while (this.#stack.length) {
          // Report all the parent `test:start` events
          const parent = ArrayPrototypePop(this.#stack);
          assert(parent.type === 'test:start');
          const msg = parent.data;
          ArrayPrototypeUnshift(this.#reported, msg);
          prefix += `${this.#indent(msg.nesting)}${symbols['arrow:right']}${msg.name}\n`;
        }
        const skippedSubtest = subtest && data.skip && data.skip !== undefined;
        const indent = this.#indent(data.nesting);
        const duration_ms = data.details?.duration_ms ? ` ${gray}(${data.details.duration_ms}ms)${white}` : '';
        const title = `${data.name}${duration_ms}${skippedSubtest ? ' # SKIP' : ''}`;
        if (this.#reported[0] && this.#reported[0].nesting === data.nesting && this.#reported[0].name === data.name) {
          // If this test has had children - it was already reported, so slightly modify the output
          ArrayPrototypeShift(this.#reported);
          return `${prefix}${indent}${color}${symbols['arrow:right']}${white}${title}\n\n`;
        }
        const error = this.#formatError(data.details?.error, indent);
        if (skippedSubtest) {
          color = gray;
          symbol = symbols['hyphen:minus'];
        }
        return `${prefix}${indent}${color}${symbol}${title}${error}${white}\n`;
      }
      case 'test:start':
        ArrayPrototypeUnshift(this.#stack, { __proto__: null, data, type });
        break;
      case 'test:diagnostic':
        return `${color}${this.#indent(data.nesting)}${symbol}${data.message}${white}\n`;
    }
  }
  _transform({ type, data }, encoding, callback) {
    callback(null, this.#handleEvent({ type, data }));
  }
}

module.exports = SpecReporter;
