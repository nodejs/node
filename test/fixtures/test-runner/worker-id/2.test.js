const test = require('node:test');

test('test2', t => {
    console.log('NODE_TEST_WORKER_ID', process.env.NODE_TEST_WORKER_ID)
});
