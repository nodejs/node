function waitFor(condition, MAX_FRAME = 500) {
  return new Promise((resolve, reject) => {
    function tick(frames) {
      // We requestAnimationFrame either for MAX_FRAME frames or until condition is
      // met.
      if (frames >= MAX_FRAME)
        reject(new Error(`Condition did not become true after ${MAX_FRAME} frames`));
      else if (condition())
        resolve();
      else
        requestAnimationFrame(() => tick(frames + 1));
    }
    tick(0);
  });
}
