try {
  new PerformanceObserver(() => true);
  postMessage("SUCCESS");
} catch (ex) {
  postMessage("FAILURE");
}
