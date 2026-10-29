/**
 * Remove the `reftest-wait` class on the document element.
 * The reftest runner will wait with taking a screenshot while
 * this class is present.
 *
 * See https://web-platform-tests.org/writing-tests/reftests.html#controlling-when-comparison-occurs
 */
function takeScreenshot() {
    document.documentElement.classList.remove("reftest-wait");
}

/**
 * Call `takeScreenshot()` after a delay of at least |timeout| milliseconds.
 * @param {number} timeout - milliseconds
 */
function takeScreenshotDelayed(timeout) {
    setTimeout(function() {
        takeScreenshot();
    }, timeout);
}

/**
 * Ensure that a precondition is met before waiting for a screenshot.
 * @param {bool} condition - Fail the test if this evaluates to false
 * @param {string} msg - Error message to write to the screenshot
 */
function failIfNot(condition, msg) {
  const fail = () => {
    (document.body || document.documentElement).textContent = `Precondition Failed: ${msg}`;
    takeScreenshot();
  };
  if (!condition) {
    if (document.readyState == "interactive") {
      fail();
    } else {
      document.addEventListener("DOMContentLoaded", fail, false);
    }
  }
}
