const fs = require('fs');

function load() {
  fs.statSync('.');
  load();
}
load();
