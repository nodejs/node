performance.mark('workerMark');
postMessage({
  'invalid' : performance.getEntriesByType('invalid').length,
  'mark' : performance.getEntriesByType('mark').length,
  'measure' : performance.getEntriesByType('measure').length
});
