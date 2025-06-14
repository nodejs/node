'use strict';

const common = require('../common');
const ArrayStream = require('../common/arraystream');
const { describe, it, before, after } = require('node:test');
const assert = require('assert');

const repl = require('repl');

function prepareREPL() {
  const input = new ArrayStream();
  const replServer = repl.start({
    prompt: '',
    input,
    output: process.stdout,
    allowBlockingCompletions: true,
  });

  // Some errors are passed to the domain, but do not callback
  replServer._domain.on('error', assert.ifError);

  return { replServer, input };
}

function testCompletion(replServer, { input, expectedCompletions }) {
  replServer.complete(
      input,
      common.mustCall((_error, data) => {
          assert.deepStrictEqual(data, [expectedCompletions, input]);
      }),
  );
};

describe('REPL tab object completion on computed properties', () => {
  describe('simple string cases', () => {
    let replServer;

    before(() => {
      const { replServer: server, input } = prepareREPL();
      replServer = server;

      input.run([
        `
          const obj = {
            one: 1,
            innerObj: { two: 2 },
            'inner object': { three: 3 },
          };

          const oneStr = 'one';
        `,
      ]);
    });

    after(() => {
      replServer.close();
    });

    it('works with double quoted strings', () => testCompletion(replServer, {
      input: 'obj["one"].toFi',
      expectedCompletions: ['obj["one"].toFixed'],
    }));

    it('works with single quoted strings', () => testCompletion(replServer, {
      input: "obj['one'].toFi",
      expectedCompletions: ["obj['one'].toFixed"],
    }));

    it('works with template strings', () => testCompletion(replServer, {
      input: 'obj[`one`].toFi',
      expectedCompletions: ['obj[`one`].toFixed'],
    }));

    it('works with nested objects', () => {
      testCompletion(replServer, {
        input: 'obj["innerObj"].tw',
        expectedCompletions: ['obj["innerObj"].two'],
      });
      testCompletion(replServer, {
        input: 'obj["innerObj"].two.tofi',
        expectedCompletions: ['obj["innerObj"].two.toFixed'],
      });
    });

    it('works with nested objects combining different type of strings', () => testCompletion(replServer, {
      input: 'obj["innerObj"][`two`].tofi',
      expectedCompletions: ['obj["innerObj"][`two`].toFixed'],
    }));

    it('works with strings with spaces', () => testCompletion(replServer, {
      input: 'obj["inner object"].th',
      expectedCompletions: ['obj["inner object"].three'],
    }));
  });
});
