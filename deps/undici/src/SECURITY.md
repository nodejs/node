# Security

## Reporting a vulnerability in undici

Report security bugs in undici via
[GitHub Security Advisories](https://github.com/nodejs/undici/security/advisories/new)
or [HackerOne](https://hackerone.com/nodejs).

Your report will normally be acknowledged within 5 days, and you will receive
a more detailed response within 10 days indicating the next steps in handling
your submission. These timelines may extend when our triage volunteers are
away, particularly at the end of the year.

After the initial reply to your report, the security team will endeavor to keep
you informed of the progress being made towards a fix and full announcement,
and may ask for additional information or guidance surrounding the reported
issue.

## Disclosure policy

* The security report is received and assigned a primary handler. The problem
  is validated against all supported versions of undici. Once confirmed, a list
  of all affected versions is determined. Code is audited to find any potential
  similar problems. Fixes are prepared for all supported releases. These fixes
  are not committed to the public repository but rather held locally pending
  the announcement.

* Because undici is bundled into Node.js, security releases are often
  coordinated with the Node.js project to avoid leaving Node.js users
  vulnerable. As a result, fixed versions of undici are published to npm
  before the corresponding CVE is disclosed, since the CVE will only be
  published after the coordinated Node.js release. This delay is typically
  a few days but can take up to a week.

## The undici threat model

Undici is an HTTP client library for Node.js. Its threat model is derived from
and aligned with the [Node.js threat model](https://github.com/nodejs/node/blob/HEAD/SECURITY.md#the-nodejs-threat-model).

### What constitutes a vulnerability

Being able to cause the following through control of the elements that undici
does not trust is considered a vulnerability:

* Disclosure or loss of integrity or confidentiality of data protected through
  the correct use of undici APIs.
* The unavailability of the runtime, including the unbounded degradation of its
  performance.

#### Denial of Service (DoS) vulnerabilities

For a behavior to be considered a DoS vulnerability, the proof of concept must
meet the following criteria:

* The API is being correctly used.
* The API is public and documented.
* The behavior is significant enough to cause a denial of service quickly
  or in a context not controlled by the application developer (for example,
  HTTP parsing).
* The behavior is directly exploitable by an untrusted source without requiring
  application mistakes.
* The behavior cannot be reasonably mitigated through standard operational
  practices (like process recycling).
* The attack demonstrates
  [asymmetric resource consumption](https://cwe.mitre.org/data/definitions/405.html),
  where the attacker expends significantly fewer resources than what is required
  by the client to process the attack.

**Undici does NOT trust**:

* Data received from the remote end of HTTP connections (both inbound responses
  and server-sent data) that is parsed or transformed by undici before being
  passed to the application. This includes:
  * HTTP response headers and status lines.
  * HTTP response bodies when processed by undici (e.g., chunked transfer
    decoding, content-encoding).
  * WebSocket frames received from a server.
  * Server-Sent Events (EventSource) data received from a server.
* TLS certificate validation performed by undici on behalf of the application.

**Undici trusts**:

* The application code that uses its APIs, including all configuration,
  options, and callbacks provided by the application.
* The operating system and its network stack.
* The Node.js runtime undici is running on.
* Dependencies installed by the application.
* The DNS resolution results provided by the operating system or configured
  resolvers.

In other words, if untrusted data passing through undici to the application
can trigger actions other than those documented for the APIs, there is likely
a security vulnerability. Examples of unwanted actions are polluting globals,
causing an unrecoverable crash, or any other unexpected side effects that can
lead to a loss of confidentiality, integrity, or availability.

### Examples of vulnerabilities

#### Improper Certificate Validation (CWE-295)

* Undici provides TLS connections to HTTPS endpoints. If certificates can be
  crafted that result in incorrect validation by undici, that is considered
  a vulnerability.

#### Inconsistent Interpretation of HTTP Responses (CWE-444)

* Undici parses HTTP responses received from servers. Bugs in parsing response
  headers or transfer encoding which can result in response smuggling or
  desynchronization are considered vulnerabilities.

#### HTTP Request Smuggling (CWE-444)

* Bugs that allow crafting requests that are interpreted differently by undici
  and an upstream server, leading to request smuggling, are considered
  vulnerabilities.

#### CRLF Injection in Request Headers (CWE-93)

* If untrusted input passed to undici APIs (such as header values or URLs)
  can inject additional headers or corrupt the HTTP request stream, that is
  considered a vulnerability.

### Examples of non-vulnerabilities

#### Malicious Third-Party Modules (CWE-1357)

* Application code and its dependencies are trusted by undici. Any scenario
  that requires a malicious third-party module cannot result in a vulnerability
  in undici.

#### Prototype Pollution Attacks (CWE-1321)

* Undici trusts the inputs provided to it by application code. It is up to the
  application to sanitize appropriately. Any scenario that requires control
  over user input passed directly by the application is not considered a
  vulnerability in undici.

#### Uncontrolled Resource Consumption on Outbound Connections (CWE-400)

* If undici is asked to connect to a remote site and the response payload is
  large enough to impact performance or cause the runtime to run out of
  resources, that is not considered a vulnerability. Applications are
  responsible for setting appropriate limits on response sizes.

#### Application Misconfiguration

* Issues arising from incorrect or insecure use of undici APIs (such as
  disabling TLS verification, ignoring errors, or passing unsanitized user
  input to request options) are the application's responsibility, not
  vulnerabilities in undici.

## Receiving security updates

Security notifications will be distributed via
[GitHub Security Advisories](https://github.com/nodejs/undici/security/advisories).

## Comments on this policy

If you have suggestions on how this process could be improved, please open an
issue on the [nodejs/undici](https://github.com/nodejs/undici/issues)
repository or file a pull request.
