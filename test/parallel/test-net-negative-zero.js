'use strict';

require('../common');

const net = require('net');

{
  const blockList = new net.BlockList();
  blockList.addSubnet('0.0.0.0', -0);
}

{
  const blockList = new net.BlockList();
  blockList.addSubnet('::', -0, 'ipv6');
}
