'use strict';

// Utilities for internet-related tests

const addresses = {
  // A generic host that has registered common DNS records,
  // supports both IPv4 and IPv6, and provides basic HTTP/HTTPS services
  INET_HOST: 'nodejs.org',
  // A host that provides IPv4 services
  INET4_HOST: 'nodejs.org',
  // A host that provides IPv6 services
  INET6_HOST: 'nodejs.org',
  // An accessible IPv4 IP,
  // defaults to the Google Public DNS IPv4 address
  INET4_IP: '8.8.8.8',
  // An accessible IPv6 IP,
  // defaults to the Google Public DNS IPv6 address
  INET6_IP: '2001:4860:4860::8888',
  // An invalid host that cannot be resolved
  // See https://tools.ietf.org/html/rfc2606#section-2
  INVALID_HOST: 'something.invalid',
  // A host with MX records registered
  MX_HOST: 'nodejs.org',
  // On some systems, .invalid returns a server failure/try again rather than
  // record not found. Use this to guarantee record not found.
  NOT_FOUND: 'come.on.fhqwhgads.test',
  // A host with SRV records registered
  SRV_HOST: '_caldav._tcp.google.com',
  // A host with PTR records registered
  PTR_HOST: '8.8.8.8.in-addr.arpa',
  // A host with NAPTR records registered
  NAPTR_HOST: 'sip2sip.info',
  // A host with SOA records registered
  SOA_HOST: 'nodejs.org',
  // A host with CAA record registered
  CAA_HOST: 'google.com',
  // A host with CNAME records registered
  CNAME_HOST: 'blog.nodejs.org',
  // A host with NS records registered
  NS_HOST: 'nodejs.org',
  // A host with TLSA records registered
  TLSA_HOST: '_443._tcp.fedoraproject.org',
  // A host with TXT records registered
  TXT_HOST: 'nodejs.org',
  // An accessible IPv4 DNS server
  DNS4_SERVER: '8.8.8.8',
  // An accessible IPv4 DNS server
  DNS6_SERVER: '2001:4860:4860::8888',
};

for (const key of Object.keys(addresses)) {
  const envName = `NODE_TEST_${key}`;
  if (process.env[envName]) {
    addresses[key] = process.env[envName];
  }
}

module.exports = {
  addresses,
};
