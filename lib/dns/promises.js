'use strict';

const dnsPromises = require('internal/dns/promises');
dnsPromises.setServers = require('dns').setServers;
module.exports = dnsPromises;
