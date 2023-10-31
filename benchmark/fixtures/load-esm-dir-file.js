(async function () {
  for (let i = 0; i < 1000; i += 1) {
    await import(`./esm-dir-file.mjs?i=${i}`);
  }
}());
