'use strict';

const { describe, it, test } = require('node:test');
const { setImmediate: setImmediatePromise } = require('node:timers/promises');

const levelOne = ['1', '2', '3'];
const levelTwo = ['1', '2', '3'];
const levelThree = ['1', '2', '3'];

test('nested-scenarios', async (t) => {
  await t.test('scenario describe-it', async (scenario) => {
    await scenario.test('describe-it block', () => {
      for (const outer of levelOne) {
        describe(`describe-it ${outer}`, () => {
          for (const middle of levelTwo) {
            describe(`describe-it ${outer}-${middle}`, () => {
              for (const inner of levelThree) {
                it(`describe-it ${outer}-${middle}-${inner}`, () => {});
              }
            });
          }
        });
      }
    });
  });

  await t.test('scenario static-no-await', async (scenario) => {
    await scenario.test('static-no-await block', (block) => {
      for (const outer of levelOne) {
        block.test(`static-no-await ${outer}`, (outerBlock) => {
          for (const middle of levelTwo) {
            outerBlock.test(`static-no-await ${outer}-${middle}`, (middleBlock) => {
              for (const inner of levelThree) {
                middleBlock.test(`static-no-await ${outer}-${middle}-${inner}`, () => {});
              }
            });
          }
        });
      }
    });
  });

  await t.test('scenario static-await', async (scenario) => {
    await scenario.test('static-await block', async (block) => {
      for (const outer of levelOne) {
        await block.test(`static-await ${outer}`, async (outerBlock) => {
          await setImmediatePromise();

          for (const middle of levelTwo) {
            await outerBlock.test(`static-await ${outer}-${middle}`, async (middleBlock) => {
              await setImmediatePromise();

              for (const inner of levelThree) {
                await middleBlock.test(`static-await ${outer}-${middle}-${inner}`, async () => {
                  await setImmediatePromise();
                });
              }
            });
          }
        });
      }
    });
  });

  await t.test('scenario mixed-await-normal', async (scenario) => {
    await scenario.test('mixed-await-normal block', (block) => {
      for (const outer of levelOne) {
        block.test(`mixed-await-normal ${outer}`, async (outerBlock) => {
          for (const middle of levelTwo) {
            outerBlock.test(`mixed-await-normal ${outer}-${middle}-sync-before`, () => {});

            await outerBlock.test(`mixed-await-normal ${outer}-${middle}-awaited`, async () => {
              await setImmediatePromise();
            });

            outerBlock.test(`mixed-await-normal ${outer}-${middle}-sync-after`, () => {});
          }
        });
      }
    });
  });
});
