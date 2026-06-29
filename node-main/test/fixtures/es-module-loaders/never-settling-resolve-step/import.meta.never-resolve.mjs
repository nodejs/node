import assert from 'node:assert';
console.log('should be output');

assert.throws(() => {
  import.meta.resolve('never-settle-resolve');
}, {
  code: 'ERR_ASYNC_LOADER_REQUEST_NEVER_SETTLED'
});
