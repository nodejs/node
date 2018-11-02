# Security

If you find a security vulnerability in Node.js, please report it to
security@nodejs.org. Please withhold public disclosure until after the security
team has addressed the vulnerability.

The security team will acknowledge your email within 24 hours. You will receive
a more detailed response within 48 hours.

There are no hard and fast rules to determine if a bug is worth reporting as a
security issue. Here are some examples of past issues and what the Security
Response Team thinks of them. When in doubt, please do send us a report
nonetheless.

## Public disclosure preferred

- [#14519](https://github.com/nodejs/node/issues/14519): _Internal domain
  function can be used to cause segfaults_. Requires the ability to execute
  arbitrary JavaScript code. That is already the highest level of privilege
  possible.

## Private disclosure preferred

- [CVE-2016-7099](https://nodejs.org/en/blog/vulnerability/september-2016-security-releases/):
  _Fix invalid wildcard certificate validation check_. This was a high-severity
  defect. It caused Node.js TLS clients to accept invalid wildcard certificates.

- [#5507](https://github.com/nodejs/node/pull/5507): _Fix a defect that makes
  the CacheBleed Attack possible_. Many, though not all, OpenSSL vulnerabilities
  in the TLS/SSL protocols also affect Node.js.

- [CVE-2016-2216](https://nodejs.org/en/blog/vulnerability/february-2016-security-releases/):
  _Fix defects in HTTP header parsing for requests and responses that can allow
  response splitting_. This was a remotely-exploitable defect in the Node.js
  HTTP implementation.

When in doubt, please do send us a report.
