'use strict';

const tls = require('tls');
const certs = tls.getCACertificates(process.env.CA_TYPE);

console.log(JSON.stringify(certs));
