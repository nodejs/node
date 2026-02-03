# Security

## Reporting a bug in Node.js

Report security bugs in Node.js via [HackerOne](https://hackerone.com/nodejs).

Normally, your report will be acknowledged within 5 days, and you'll receive
a more detailed response to your report within 10 days indicating the
next steps in handling your submission. These timelines may extend when
our triage volunteers are away on holiday, particularly at the end of the
year.

After the initial reply to your report, the security team will endeavor to keep
you informed of the progress being made towards a fix and full announcement,
and may ask for additional information or guidance surrounding the reported
issue.

If you do not receive an acknowledgement of your report within 6 business
days, or if you cannot find a private security contact for the project, you
may escalate to the OpenJS Foundation CNA at `security@lists.openjsf.org`.

If the project acknowledges your report but does not provide any further
response or engagement within 14 days, escalation is also appropriate.

### Node.js bug bounty program

The Node.js project engages in an official bug bounty program for security
researchers and responsible public disclosures.  The program is managed through
the HackerOne platform. See <https://hackerone.com/nodejs> for further details.

## Reporting a bug in a third-party module

Security bugs in third-party modules should be reported to their respective
maintainers.

## Disclosure policy

Here is the security disclosure policy for Node.js

* The security report is received and is assigned a primary handler. This
  person will coordinate the fix and release process. The problem is validated
  against all supported Node.js versions. Once confirmed, a list of all affected
  versions is determined. Code is audited to find any potential similar
  problems. Fixes are prepared for all supported releases.
  These fixes are not committed to the public repository but rather held locally
  pending the announcement.

* A suggested embargo date for this vulnerability is chosen and a CVE (Common
  Vulnerabilities and Exposures (CVE®)) is requested for the vulnerability.

* On the embargo date, a copy of the announcement is sent to the Node.js
  security mailing list. The changes are pushed to the public repository and new
  builds are deployed to nodejs.org. Within 6 hours of the mailing list being
  notified, a copy of the advisory will be published on the Node.js blog.

* Typically, the embargo date will be set 72 hours from the time the CVE is
  issued. However, this may vary depending on the severity of the bug or
  difficulty in applying a fix.

* This process can take some time, especially when we need to coordinate with
  maintainers of other projects. We will try to handle the bug as quickly as
  possible; however, we must follow the release process above to ensure that we
  handle disclosure consistently.

## Code of Conduct and Vulnerability Reporting Guidelines

When reporting security vulnerabilities, reporters must adhere to the following guidelines:

1. **Code of Conduct Compliance**: All security reports must comply with our
   [Code of Conduct](CODE_OF_CONDUCT.md). Reports that violate our code of conduct
   will not be considered and may result in being banned from future participation.

2. **No Harmful Actions**: Security research and vulnerability reporting must not:
   * Cause damage to running systems or production environments.
   * Disrupt Node.js development or infrastructure.
   * Affect other users' applications or systems.
   * Include actual exploits that could harm users.
   * Involve social engineering or phishing attempts.

3. **Responsible Testing**: When testing potential vulnerabilities:
   * Use isolated, controlled environments.
   * Do not test on production systems without prior authorization. Contact
     the Node.js Technical Steering Committee (<tsc@iojs.org>) for permission or open
     a HackerOne report.
   * Do not attempt to access or modify other users' data.
   * Immediately stop testing if unauthorized access is gained accidentally.

4. **Report Quality**
   * Provide clear, detailed steps to reproduce the vulnerability.
   * Include only the minimum proof of concept required to demonstrate the issue.
   * Remove any malicious payloads or components that could cause harm.

Failure to follow these guidelines may result in:

* Rejection of the vulnerability report.
* Forfeiture of any potential bug bounty.
* Temporary or permanent ban from the bug bounty program.
* Legal action in cases of malicious intent.

## The Node.js threat model

In the Node.js threat model, there are trusted elements such as the
underlying operating system. Vulnerabilities that require the compromise
of these trusted elements are outside the scope of the Node.js threat
model.

For a vulnerability to be eligible for a bug bounty, it must be a
vulnerability in the context of the Node.js threat model. In other
words, it cannot assume that a trusted element (such as the operating
system) has been compromised.

### Experimental platforms

Node.js maintains a tier-based support system for operating systems and
hardware combinations (Tier 1, Tier 2, and Experimental). For platforms
classified as "Experimental" in the [supported platforms](BUILDING.md#supported-platforms)
documentation:

* Security vulnerabilities that only affect experimental platforms will **not** be accepted as valid security issues.
* Any issues on experimental platforms will be treated as normal bugs.
* No CVEs will be issued for issues that only affect experimental platforms
* Bug bounty rewards are not available for experimental platform-specific issues

This policy recognizes that experimental platforms may not compile, may not
pass the test suite, and do not have the same level of testing and support
infrastructure as Tier 1 and Tier 2 platforms.

### Experimental features behind compile-time flags

Node.js includes certain experimental features that are only available when
Node.js is compiled with specific flags. These features are intended for
development, debugging, or testing purposes and are not enabled in official
releases.

* Security vulnerabilities that only affect features behind compile-time flags
  will **not** be accepted as valid security issues.
* Any issues with these features will be treated as normal bugs.
* No CVEs will be issued for issues that only affect compile-time flag features.
* Bug bounty rewards are not available for compile-time flag feature issues.

This policy recognizes that experimental features behind compile-time flags
are not ready for public consumption and may have incomplete implementations,
missing security hardening, or other limitations that make them unsuitable
for production use.

### What constitutes a vulnerability

Being able to cause the following through control of the elements that Node.js
does not trust is considered a vulnerability:

* Disclosure or loss of integrity or confidentiality of data protected through
  the correct use of Node.js APIs.
* The unavailability of the runtime, including the unbounded degradation of its
  performance.
* Memory leaks qualify as vulnerabilities when all of the following criteria are met:
  * The API is being correctly used.
  * The API doesn't have a warning against its usage in a production environment.
  * The API is public and documented.
  * The API is on stable (2.0) status.
  * The memory leak is significant enough to cause a denial of service quickly
    or in a context not controlled by the user (for example, HTTP parsing).
  * The memory leak is directly exploitable by an untrusted source without requiring application mistakes.
  * The leak cannot be reasonably mitigated through standard operational practices (like process recycling).
  * The leak occurs deterministically under normal usage patterns rather than edge cases.
  * The leak occurs at a rate that would cause practical resource exhaustion within a practical timeframe under
    typical workloads.
  * The attack demonstrates [asymmetric resource consumption](https://cwe.mitre.org/data/definitions/405.html),
    where the attacker expends significantly fewer resources than what's required by the server to process the
    attack. Attacks requiring comparable resources on the attacker's side (which can be mitigated through common
    practices like rate limiting) may not qualify.

If Node.js loads configuration files or runs code by default (without a
specific request from the user), and this is not documented, it is considered a
vulnerability.
Vulnerabilities related to this case may be fixed by a documentation update.

**Node.js does NOT trust**:

* Data received from the remote end of inbound network connections
  that are accepted through the use of Node.js APIs and
  which is transformed/validated by Node.js before being passed
  to the application. This includes:
  * HTTP APIs (all flavors) server APIs.
* The data received from the remote end of outbound network connections
  that are created through the use of Node.js APIs and
  which is transformed/validated by Node.js before being passed
  to the application **except** with respect to payload length. Node.js trusts
  that applications make connections/requests which will avoid payload
  sizes that will result in a Denial of Service.
  * HTTP APIs (all flavors) client APIs.
  * DNS APIs.
* Consumers of data protected through the use of Node.js APIs (for example,
  people who have access to data encrypted through the Node.js crypto APIs).
* The file content or other I/O that is opened for reading or writing by the
  use of Node.js APIs (ex: stdin, stdout, stderr).

In other words, if the data passing through Node.js to/from the application
can trigger actions other than those documented for the APIs, there is likely
a security vulnerability. Examples of unwanted actions are polluting globals,
causing an unrecoverable crash, or any other unexpected side effects that can
lead to a loss of confidentiality, integrity, or availability.

For example, if trusted input (like secure application code) is correct,
then untrusted input must not lead to arbitrary JavaScript code execution.

**Node.js trusts everything else**. Examples include:

* The developers and infrastructure that run it.
* The operating system that Node.js is running under and its configuration,
  along with anything under the control of the operating system.
* The code it is asked to run, including JavaScript, WASM and native code, even
  if said code is dynamically loaded, e.g., all dependencies installed from the
  npm registry.
  The code run inherits all the privileges of the execution user.
* Inputs provided to it by the code it is asked to run, as it is the
  responsibility of the application to perform the required input validations,
  e.g. the input to `JSON.parse()`.
* Any connection used for inspector (debugger protocol) regardless of being
  opened by command line options or Node.js APIs, and regardless of the remote
  end being on the local machine or remote.
* The file system when requiring a module.
  See <https://nodejs.org/api/modules.html#all-together>.
* The `node:wasi` module does not currently provide the comprehensive file
  system security properties provided by some WASI runtimes.
* The execution path is trusted. Additionally, Node.js path manipulation functions
  such as `path.join()` and `path.normalize()` trust their input. Reports about issues
  related to these functions that rely on unsanitized input are not considered vulnerabilities
  requiring CVEs, as it's the user's responsibility to sanitize path inputs according to
  their security requirements.

Any unexpected behavior from the data manipulation from Node.js Internal
functions may be considered a vulnerability if they are exploitable via
untrusted resources.

In addition to addressing vulnerabilities based on the above, the project works
to avoid APIs and internal implementations that make it "easy" for application
code to use the APIs incorrectly in a way that results in vulnerabilities within
the application code itself. While we don’t consider those vulnerabilities in
Node.js itself and will not necessarily issue a CVE, we do want them to be
reported privately to Node.js first.
We often choose to work to improve our APIs based on those reports and issue
fixes either in regular or security releases depending on how much of a risk to
the community they pose.

### Examples of vulnerabilities

#### Improper Certificate Validation (CWE-295)

* Node.js provides APIs to validate handling of Subject Alternative Names (SANs)
  in certificates used to connect to a TLS/SSL endpoint. If certificates can be
  crafted that result in incorrect validation by the Node.js APIs that is
  considered a vulnerability.

#### Inconsistent Interpretation of HTTP Requests (CWE-444)

* Node.js provides APIs to accept HTTP connections. Those APIs parse the
  headers received for a connection and pass them on to the application.
  Bugs in parsing those headers which can result in request smuggling are
  considered vulnerabilities.

#### Missing Cryptographic Step (CWE-325)

* Node.js provides APIs to encrypt data. Bugs that would allow an attacker
  to get the original data without requiring the decryption key are
  considered vulnerabilities.

#### External Control of System or Configuration Setting (CWE-15)

* If Node.js automatically loads a configuration file that is not documented
  and modification of that configuration can affect the confidentiality of
  data protected using the Node.js APIs, then this is considered a vulnerability.

### Examples of non-vulnerabilities

#### Malicious Third-Party Modules (CWE-1357)

* Code is trusted by Node.js. Therefore any scenario that requires a malicious
  third-party module cannot result in a vulnerability in Node.js.

#### Prototype Pollution Attacks (CWE-1321)

* Node.js trusts the inputs provided to it by application code.
  It is up to the application to sanitize appropriately. Therefore any scenario
  that requires control over user input is not considered a vulnerability.

#### Uncontrolled Search Path Element (CWE-427)

* Node.js trusts the file system in the environment accessible to it.
  Therefore, it is not a vulnerability if it accesses/loads files from any path
  that is accessible to it.

#### External Control of System or Configuration Setting (CWE-15)

* If Node.js automatically loads a configuration file that is documented,
  no scenario that requires modification of that configuration file is
  considered a vulnerability.

#### Uncontrolled Resource Consumption (CWE-400) on outbound connections

* If Node.js is asked to connect to a remote site and return an
  artifact, it is not considered a vulnerability if the size of
  that artifact is large enough to impact performance or
  cause the runtime to run out of resources.

#### Vulnerabilities affecting software downloaded by Corepack

* Corepack defaults to downloading the latest version of the software requested
  by the user, or a specific version requested by the user. For this reason,
  Node.js releases won't be affected by such vulnerabilities. Users are
  responsible for keeping the software they use through Corepack up-to-date.

#### Exposing Application-Level APIs to Untrusted Users (CWE-653)

* Node.js trusts the application code that uses its APIs. When application code
  exposes Node.js functionality to untrusted users in an unsafe manner, any
  resulting crashes, data corruption, or other issues are not considered
  vulnerabilities in Node.js itself. It is the application's responsibility to:
  * Validate and sanitize all untrusted input before passing it to Node.js APIs.
  * Design appropriate access controls and security boundaries.
  * Avoid exposing low-level or dangerous APIs directly to untrusted users.

* Examples of scenarios that are **not** Node.js vulnerabilities:
  * Allowing untrusted users to register SQLite user-defined functions that can
    perform arbitrary operations (e.g., closing database connections during query
    execution, causing crashes or use-after-free conditions).
  * Exposing `child_process.exec()` or similar APIs to untrusted users without
    proper input validation, allowing command injection.
  * Allowing untrusted users to control file paths passed to file system APIs
    without validation, leading to path traversal issues.
  * Permitting untrusted users to define custom code that executes with the
    application's privileges (e.g., custom transforms, plugins, or callbacks).

* These scenarios represent application-level security issues, not Node.js
  vulnerabilities. The root cause is the application's failure to establish
  proper security boundaries between trusted application logic and untrusted
  user input.

## Assessing experimental features reports

Experimental features are eligible for security reports just like any other
stable feature of Node.js. They may also receive the same severity score that a
stable feature would.

## Receiving security updates

Security notifications will be distributed via the following methods.

* <https://groups.google.com/group/nodejs-sec>
* <https://nodejs.org/en/blog/vulnerability>

### CVE publication timeline

When security releases are published, there is a built-in delay before the
corresponding CVEs are publicly disclosed. This delay occurs because:

1. After the security release, we request the vulnerability reporter to disclose
   the details on HackerOne.
2. If the reporter does not disclose within one day, we proceed with forced
   disclosure to publish the CVEs.
3. The disclosure then goes through HackerOne's approval process before the CVEs
   become publicly available.

As a result, CVEs may not be immediately available when security releases are
published, but will typically be disclosed within a few days of the release.

## Comments on this policy

If you have suggestions on how this process could be improved, please visit
the [nodejs/security-wg](https://github.com/nodejs/security-wg)
repository.

## Incident Response Plan

In the event of a security incident, please refer to the
[Security Incident Response Plan](https://github.com/nodejs/security-wg/blob/main/INCIDENT_RESPONSE_PLAN.md).

## Node.js Security Team

Node.js security team members are expected to keep all information that they
have privileged access to by being on the team completely private to the team.
This includes agreeing to not notify anyone outside the team of issues that have
not yet been disclosed publicly, including the existence of issues, expectations
of upcoming releases, and patching of any issues other than in the process of
their work as a member of the security team.

### Node.js Security Team Membership Policy

The Node.js Security Team has access to security-sensitive issues and patches
that aren't appropriate for public availability.

The policy for inclusion is as follows:

1. All members of @nodejs/TSC have access to private security reports and
   private patches.
2. Members of the @nodejs/releasers team
   have access to private security patches in order to produce releases.
3. On a case-by-case basis, individuals outside the Technical Steering
   Committee are invited by the TSC to have access to private security reports
   or private patches so that their expertise can be applied to an issue or
   patch. This access may be temporary or permanent, as decided by the TSC.

Membership on the security teams can be requested via an issue in the TSC repo.

## Team responsible for Triaging security reports

The responsibility of Triage is to determine whether Node.js must take any
action to mitigate the issue, and if so, to ensure that the action is taken.

Mitigation may take many forms, for example, a Node.js security release that
includes a fix, documentation, an informational CVE or blog post.

* [@mcollina](https://github.com/mcollina) - Matteo Collina
* [@RafaelGSS](https://github.com/RafaelGSS) - Rafael Gonzaga
* [@vdeturckheim](https://github.com/vdeturckheim) - Vladimir de Turckheim
* [@BethGriggs](https://github.com/BethGriggs) - Beth Griggs

## Team with access to private security reports against Node.js

[TSC voting members](https://github.com/nodejs/node#tsc-voting-members)
have access.

In addition, these individuals have access:

* [BethGriggs](https://github.com/BethGriggs) - **Beth Griggs**
* [MylesBorins](https://github.com/MylesBorins) -  **Myles Borins**
* [bengl](https://github.com/bengl)- **Bryan English**
* [bnoordhuis](https://github.com/bnoordhuis) **Ben Noordhuis**
* [cjihrig](https://github.com/cjihrig) **Colin Ihrig**
* [joesepi](https://github.com/joesepi) - **Joe Sepi**
* [juanarbol](https://github.com/juanarbol) **Juan Jose Arboleda**
* [ulisesgascon](https://github.com/ulisesgascon) **Ulises Gascón**
* [vdeturckheim](https://github.com/vdeturckheim) - **Vladimir de Turckheim**

The list is from the [member page](https://hackerone.com/organizations/nodejs/settings/users) for
the Node.js program on HackerOne.

## Team with access to private security patches to Node.js

<!-- ncu-team-sync.team(nodejs-private/security) -->

* [@aduh95](https://github.com/aduh95) - Antoine du Hamel
* [@anonrig](https://github.com/anonrig) - Yagiz Nizipli
* [@bengl](https://github.com/bengl) - Bryan English
* [@benjamingr](https://github.com/benjamingr) - Benjamin Gruenbaum
* [@bmeck](https://github.com/bmeck) - Bradley Farias
* [@bnoordhuis](https://github.com/bnoordhuis) - Ben Noordhuis
* [@BridgeAR](https://github.com/BridgeAR) - Ruben Bridgewater
* [@gireeshpunathil](https://github.com/gireeshpunathil) - Gireesh Punathil
* [@guybedford](https://github.com/guybedford) - Guy Bedford
* [@indutny](https://github.com/indutny) - Fedor Indutny
* [@jasnell](https://github.com/jasnell) - James M Snell
* [@joaocgreis](https://github.com/joaocgreis) - João Reis
* [@joesepi](https://github.com/joesepi) - Joe Sepi
* [@joyeecheung](https://github.com/joyeecheung) - Joyee Cheung
* [@juanarbol](https://github.com/juanarbol) - Juan José
* [@legendecas](https://github.com/legendecas) - Chengzhong Wu
* [@marco-ippolito](https://github.com/marco-ippolito) - Marco Ippolito
* [@mcollina](https://github.com/mcollina) - Matteo Collina
* [@MoLow](https://github.com/MoLow) - Moshe Atlow
* [@panva](https://github.com/panva) - Filip Skokan
* [@RafaelGSS](https://github.com/RafaelGSS) - Rafael Gonzaga
* [@richardlau](https://github.com/richardlau) - Richard Lau
* [@ronag](https://github.com/ronag) - Robert Nagy
* [@ruyadorno](https://github.com/ruyadorno) - Ruy Adorno
* [@santigimeno](https://github.com/santigimeno) - Santiago Gimeno
* [@ShogunPanda](https://github.com/ShogunPanda) - Paolo Insogna
* [@targos](https://github.com/targos) - Michaël Zasso
* [@tniessen](https://github.com/tniessen) - Tobias Nießen
* [@UlisesGascon](https://github.com/UlisesGascon) - Ulises Gascón
* [@vdeturckheim](https://github.com/vdeturckheim) - Vladimir de Turckheim

<!-- ncu-team-sync end -->
