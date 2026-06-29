const err = new Error('err with wrong stack');

setImmediate(() => {
  (async () => {
    await 1;
    throw err;
  })();
});
