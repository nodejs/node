# Node.js Security Policy

## Reporting Vulnerabilities

### Node.js Core
Report security issues through [HackerOne](https://hackerone.com/nodejs).  
**Acknowledgement:** Within 5 days.  
**Response with next steps:** Within 10 days.  

If no acknowledgement is received within 6 business days, escalate to `security@lists.openjsf.org`. Escalation is also appropriate if no further engagement occurs within 14 days.

### Third-Party Modules
Report security issues directly to the module’s maintainers.

### Bug Bounty
Node.js participates in an official bug bounty program via HackerOne. Details at [https://hackerone.com/nodejs](https://hackerone.com/nodejs).

---

## Disclosure Policy
* Reports are assigned to a primary handler to validate, audit, and coordinate fixes across all supported versions.
* Fixes are prepared privately and only released publicly on the embargo date, along with a CVE.
* Embargo is typically 72 hours from CVE issuance, subject to severity and coordination with other projects.

---

## Reporting Guidelines
1. **Comply with Code of Conduct**: See [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md).  
2. **No Harmful Actions**: Avoid damaging production systems, other users’ data, or including exploits.  
3. **Responsible Testing**: Use isolated environments; stop immediately if unauthorized access occurs.  
4. **Report Quality**: Provide clear, minimal proof-of-concept steps.

*Violations may result in rejection, bounty forfeiture, program bans, or legal action.*

---

## Node.js Threat Model
**Node.js trusts:**  
* Operating system and environment.  
* Application code, including JavaScript, WASM, and native modules.  
* Input validated by the application (e.g., JSON).  

**Node.js does not trust:**  
* Untrusted input via Node.js APIs that could trigger unintended behavior.  
* Data that could compromise confidentiality, integrity, or availability.  

Vulnerabilities must be exploitable without assuming compromise of trusted elements.

---

## Experimental Platforms
* Vulnerabilities affecting only experimental platforms are not eligible for CVEs or bounties.  
* Such issues are treated as regular bugs.

---

## Examples

**Valid Vulnerabilities**  
* Improper TLS certificate validation.  
* HTTP request parsing errors leading to request smuggling.  
* Missing cryptographic steps.  
* Undocumented automatic configuration loading affecting security.

**Non-Vulnerabilities**  
* Malicious third-party modules.  
* Prototype pollution in application-supplied data.  
* Unsafe file paths in trusted environments.  
* Outbound resource consumption outside Node.js control.  

---

## Security Updates
Notifications are distributed via:  
* [Node.js Security Google Group](https://groups.google.com/group/nodejs-sec)  
* [Node.js Vulnerability Blog](https://nodejs.org/en/blog/vulnerability)  
