const {describe, test, beforeEach, afterEach} = require("node:test");
const {setTimeout} = require("timers/promises");


describe('before each timeout', () => {
  let i = 0;

  beforeEach(async () => {
    if (i++ === 0) {
      console.log('gonna timeout')
      await setTimeout(50);
      return;
    }
    console.log('not gonna timeout');
  }, {timeout: 20});

  test('first describe first test', () => {
    console.log('before each test first ' + i);
  });

  test('first describe second test', () => {
    console.log('before each test second ' + i);
  });
});


describe('after each timeout', () => {
  let i = 0;

  afterEach(async () => {
    if (i++ === 0) {
      console.log('gonna timeout')
      await setTimeout(50);
      return;
    }
    console.log('not gonna timeout');
  }, {timeout: 20});

  test('second describe first test', () => {
    console.log('after each test first ' + i);
  });

  test('second describe second test', () => {
    console.log('after each test second ' + i);
  });
});
