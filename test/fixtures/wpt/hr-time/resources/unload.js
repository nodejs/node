const syncDelay = ms => {
  const start = performance.now();
  let elapsedTime;
  do {
    elapsedTime = performance.now() - start;
  } while (elapsedTime < ms);
};

const markTime = (docName, lifecycleEventName) => {
  // Calculating these values before the below `mark` invocation ensures that delays in
  // reaching across to the other window object doesn't interfere with the correctness
  // of the test.
  const dateNow = Date.now();
  const performanceNow = performance.now();

  window.opener.mark({
    docName,
    lifecycleEventName,
    performanceNow: performanceNow,
    dateNow: dateNow
  });
};

const setupUnloadPrompt = (docName, msg) => {
  window.addEventListener("beforeunload", ev => {
    markTime(docName, "beforeunload");
    return ev.returnValue = msg || "Click OK to continue test."
  });
};

const setupListeners = (docName, nextDocument) => {
  window.addEventListener("load", () => {
    markTime(docName, "load");
    document.getElementById("proceed").addEventListener("click", ev => {
      ev.preventDefault();
      if (nextDocument) {
        document.location = nextDocument;
      } else {
        window.close();
      }
    })
  });

  setupUnloadPrompt(docName);

  window.addEventListener("unload", () => {
    markTime(docName, "unload");
    if (docName !== "c") { syncDelay(1000); }
  });
};

