setImmediate(() => {
  (async () => {
    await 1;
    throw null;
  })();
});
