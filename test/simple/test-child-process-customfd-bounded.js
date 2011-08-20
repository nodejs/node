var common = require('../common');
var spawn = require('child_process').spawn;
var bigish = Array(200);

for (var i = 0, il = bigish.length; i < il; ++i)
 bigish[i] = -1;

spawn('/bin/echo', [], { customFds: bigish });

