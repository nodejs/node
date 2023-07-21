module.exports = {
  waitForAbort: function ({ testNumber, signal }) {
    let retries = 0;

    const interval = setInterval(() => {
      retries++;
      if(signal.aborted) {
        console.log(`abort called for test ${testNumber}`);
        clearInterval(interval);
        return;
      }

      if(retries > 100) {
        clearInterval(interval);
        throw new Error(`abort was not called for test ${testNumber}`);
      }
    }, 10);
  }
}
