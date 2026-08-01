setup({ allow_uncaught_exception:true });

// For SyntaxError, the line in doImportScripts() is expected to be reported
// as `e.lineno` etc. below.
// doImportScripts() is introduced here to prevent the line number from being
// affected by changes in runTest(), use of setTimeout(), etc.
function doImportScripts(url) {
  importScripts(url);
}

const t0 = async_test("WorkerGlobalScope error event: error");
const t1 = async_test("WorkerGlobalScope error event: message");
const t2 = async_test("WorkerGlobalScope error event: filename");
const t3 = async_test("WorkerGlobalScope error event: lineno");

function runTest(importScriptUrl, shouldUseSetTimeout, expected) {
  self.addEventListener("error", e => {
    if (expected === "NetworkError") {
      t0.step_func_done(() => {
        assert_equals(e.error.constructor, DOMException,
            "e.error should be a DOMException")
        assert_equals(e.error.name, "NetworkError");
      })();

      t1.step_func_done(() => {
        assert_not_equals(e.message, "Script error.",
            "e.message should not be muted to 'Script error.'");
      })();

      // filename, lineno etc. should NOT point to the location within
      // `syntax-error.js` (otherwise parse errors to be muted are
      // leaked to JavaScript).
      // we expect they point to the caller of `importScripts()`,
      // while this is not explicitly stated in the spec.
      t2.step_func_done(() => {
        assert_equals(e.filename, self.location.origin +
            '/workers/interfaces/WorkerUtils/importScripts/report-error-helper.js');
      })();
      t3.step_func_done(() => {
        assert_equals(e.lineno, 8);
      })();
      // We don't check `e.colno` for now, because browsers set different
      // `colno` values.
    } else if (expected === "SyntaxError") {
      t0.step_func_done(() => {
        assert_equals(e.error.constructor, SyntaxError);
        assert_equals(e.error.name, "SyntaxError");
      })();

      t1.step_func_done(() => {
        assert_not_equals(e.message, "Script error.",
            "e.message should not be muted to 'Script error.'");
      })();

      // filename, lineno etc. are expected to point to the location within
      // `syntax-error.js` because it is same-origin,
      // while this is not explicitly stated in the spec.
      t2.step_func_done(() => {
        assert_equals(e.filename, self.location.origin +
            '/workers/modules/resources/syntax-error.js');
      })();
      t3.step_func_done(() => {
        assert_equals(e.lineno, 1);
      })();
      // We don't check `e.colno` for now, because browsers set different
      // `colno` values.
    }

    // Because importScripts() throws, we call done() here.
    done();
  });

  if (shouldUseSetTimeout) {
    setTimeout(
      () => doImportScripts(importScriptUrl),
      0);
  } else {
    doImportScripts(importScriptUrl);
  }
}
