const { version } = require('process');

(async () => {
  await import('./print-version.js');
})();
