'use strict';
var common = require('../common');

var bigish = Array(200);

for (var i = 0, il = bigish.length; i < il; ++i)
  bigish[i] = -1;

common.spawnPwd({ customFds: bigish });
