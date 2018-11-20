var fs = require('fs');

var file_name = process.argv[2];
var file_size = parseInt(process.argv[3]);

var fd = fs.openSync(file_name, 'w');
fs.ftruncateSync(fd, file_size);
fs.closeSync(fd);
