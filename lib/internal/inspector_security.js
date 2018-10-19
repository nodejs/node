'use strict';

const dns = require('dns');
const util = require('util');

const lookup = util.promisify(dns.lookup);

const IP_RANGES = {
  local: 'LOCAL',
  private: 'PRIVATE',
  public: 'PUBLIC'
};

function isValidIpV4(parts) {
  return parts.length === 4 &&
    (parts.every((part) => part >= 0 && part <= 255));
}

function convertPartsToLong(parts) {
  return parts[0] * 1e9 + parts[1] * 1e6 + parts[2] * 1e3 + parts[3];
}

// Loopback: 127.0.0.0 - 127.255.255.255.
// Private: 10.0.0.0 - 10.255.255.255, 172.16.0.0 - 172.31.255.255
//          192.168.0.0 - 192.168.255.255
// Public: everything else
function toRange(ip) {
  if (ip >= 127000000000 && ip <= 127255255255) {
    return IP_RANGES.local;
  } else if ((ip >= 10000000000 && ip <= 10255255255) ||
    (ip >= 172016000000 && ip <= 172031255255) ||
    (ip >= 192168000000 && ip <= 192168255255)) {
    return IP_RANGES.private;
  }
  return IP_RANGES.public;
}

async function checkInspectorHost(host) {
  let parts = host.split('.').map((part) => parseInt(part, 10));

  if (!isValidIpV4(parts)) {
    try {
      parts = await lookup(host);
    } catch (e) {
      return `Inspector: could not determinate the ip of ${host}`;
    }
  } else {
    return '';
  }

  const ip = convertPartsToLong(parts);
  const range = toRange(ip);

  if (range === IP_RANGES.local) {
    return '';
  } else if (range === IP_RANGES.private) {
    return 'Inspector: you are running inspector on a private network. ' +
    'Make sure you trust all the hosts on this network ' +
    'or filter out traffic on your firewall';
  } else {
    return 'Inspector: you are running inspector on a PUBLIC network. ' +
    'This is a high security risk, anyone who has access to your computer ' +
    'can run arbitrary code on your machine.';
  }
}

module.exports = { checkInspectorHost };
