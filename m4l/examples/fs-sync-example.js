const fs = require('fs');
const path = require('path');

const data = fs.readFileSync(path.resolve(__dirname, 'fs-sync-example.js'));

console.log(data);
