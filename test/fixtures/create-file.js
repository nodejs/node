const fs = require('fs');

const file_name = process.argv[2];
const file_size = parseInt(process.argv[3]);

const fd = fs.openSync(file_name, 'w');
fs.ftruncateSync(fd, file_size);
fs.closeSync(fd);
