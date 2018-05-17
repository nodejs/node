'use strict';

const fs = require('fs');
fs.readFile(0, (err, data) => {
  console.log(data.toString());
});
