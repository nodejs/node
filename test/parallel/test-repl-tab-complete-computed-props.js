'use strict';

require('../common');
const { startNewREPLServer } = require('../common/repl');
const { describe, it, before, after } = require('node:test');
const assert = require('assert');

const testCompletion = (replServer, { input, expectedCompletions }) =>
  new Promise((resolve, reject) => {
    // No need to wrap in `mustCall`, this test won't
    // exit until this function is falled
    replServer.complete(input, (error, data) => {
      try {
        // eslint-disable-next-line node-core/must-call-assert
        assert.ifError(error);
        // eslint-disable-next-line node-core/must-call-assert
        assert.deepStrictEqual(data, [expectedCompletions, input]);
      } catch (err) {
        reject(err);
      }
      resolve();
    });
  });

describe('REPL tab object completion on computed properties', () => {
  describe('simple string cases', () => {
    let replServer;

    before(async () => {
      const { replServer: server, run } = startNewREPLServer();
      replServer = server;

      await run([
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

    it('works with double quoted strings', () =>
      testCompletion(replServer, {
        input: 'obj["one"].toFi',
        expectedCompletions: ['obj["one"].toFixed'],
      }));

    it('works with single quoted strings', () =>
      testCompletion(replServer, {
        input: "obj['one'].toFi",
        expectedCompletions: ["obj['one'].toFixed"],
      }));

    it('works with template strings', () =>
      testCompletion(replServer, {
        input: 'obj[`one`].toFi',
        expectedCompletions: ['obj[`one`].toFixed'],
      }));

    it('works with nested objects', async () => {
      await testCompletion(replServer, {
        input: 'obj["innerObj"].tw',
        expectedCompletions: ['obj["innerObj"].two'],
      });
      await testCompletion(replServer, {
        input: 'obj["innerObj"].two.tofi',
        expectedCompletions: ['obj["innerObj"].two.toFixed'],
      });
    });

    it('works with nested objects combining different type of strings', () =>
      testCompletion(replServer, {
        input: 'obj["innerObj"][`two`].tofi',
        expectedCompletions: ['obj["innerObj"][`two`].toFixed'],
      }));

    it('works with strings with spaces', () =>
      testCompletion(replServer, {
        input: 'obj["inner object"].th',
        expectedCompletions: ['obj["inner object"].three'],
      }));
  });

  describe('variables as indexes', () => {
    let replServer;

    before(async () => {
      const { replServer: server, run } = startNewREPLServer();
      replServer = server;

      await run([
        `
          const oneStr = 'One';
          const helloWorldStr = 'Hello' + ' ' + 'World';

          const obj = {
            [oneStr]: 1,
            ['Hello World']: 'hello world!',
          };

          const lookupObj = {
            stringLookup: helloWorldStr,
            ['number lookup']: oneStr,
          };
        `,
      ]);
    });

    after(() => {
      replServer.close();
    });

    it('works with a simple variable', () =>
      testCompletion(replServer, {
        input: 'obj[oneStr].toFi',
        expectedCompletions: ['obj[oneStr].toFixed'],
      }));

    it('works with a computed variable', () =>
      testCompletion(replServer, {
        input: 'obj[helloWorldStr].tolocaleup',
        expectedCompletions: ['obj[helloWorldStr].toLocaleUpperCase'],
      }));

    it('works with a simple inlined computed property', () =>
      testCompletion(replServer, {
        input: 'obj["Hello " + "World"].tolocaleup',
        expectedCompletions: ['obj["Hello " + "World"].toLocaleUpperCase'],
      }));

    it('works with a ternary inlined computed property', () =>
      testCompletion(replServer, {
        input:
          'obj[(1 + 2 > 5) ? oneStr : "Hello " + "World"].toLocaleUpperCase',
        expectedCompletions: [
          'obj[(1 + 2 > 5) ? oneStr : "Hello " + "World"].toLocaleUpperCase',
        ],
      }));

    it('works with an inlined computed property with a nested property lookup', () =>
      testCompletion(replServer, {
        input: 'obj[lookupObj.stringLookup].tolocaleupp',
        expectedCompletions: ['obj[lookupObj.stringLookup].toLocaleUpperCase'],
      }));

    it('works with an inlined computed property with a nested inlined computer property lookup', () =>
      testCompletion(replServer, {
        input: 'obj[lookupObj["number" + " lookup"]].toFi',
        expectedCompletions: ['obj[lookupObj["number" + " lookup"]].toFixed'],
      }));
  });
});
