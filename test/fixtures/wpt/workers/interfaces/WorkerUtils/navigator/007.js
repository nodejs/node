var log = [];
var neverEncounteredValue = "This is not the value you are looking for.";
for (x in navigator) {
  // skip functions, as they are writable
  if (typeof navigator[x] === 'function') continue;
  // this should silently fail and not throw per webidl
  navigator[x] = neverEncounteredValue;
  if (navigator[x] === neverEncounteredValue)
    log.push(x);
}
postMessage(log.join(', '));