# Security release process

The security release process covers the steps required to plan/implement a
security release. This document is copied into the description of the Next
Security Release and used to track progress on the release. It contains _**TEXT
LIKE THIS**_ which will be replaced during the release process with the
information described.

## Security release stewards

For each security release, a security steward will take ownership for
coordinating the steps outlined in this process. Security stewards
are nominated through an issue in the TSC repository and approved
through the regular TSC consensus process. Once approved, they
are given access to all of the resources needed to carry out the
steps listed in the process as outlined in
[security steward on/off boarding](security-steward-on-off-boarding.md).

The current security stewards are documented in the main Node.js
[README.md](https://github.com/nodejs/node#security-release-stewards).

| Company      | Person          | Release Date |
| ------------ | --------------- | ------------ |
| NearForm     | Matteo          | 2021-Oct-12  |
| Datadog      | Bryan           | 2022-Jan-10  |
| RH and IBM   | Joe             | 2022-Mar-18  |
| NearForm     | Matteo / Rafael | 2022-Jul-07  |
| Datadog      | Vladimir        | 2022-Sep-23  |
| NodeSource   | Juan            | 2022-Nov-04  |
| RH and IBM   | Michael         | 2023-Feb-16  |
| NearForm     | Rafael          | 2023-Jun-20  |
| NearForm     | Rafael          | 2023-Aug-09  |
| NearForm     | Rafael          | 2023-Oct-13  |
| Datadog      | Bryan           |              |
| IBM          | Joe             |              |
| Platformatic | Matteo          |              |
| NodeSource   | Juan            |              |
| Red Hat      | Michael         |              |

## Planning

* [ ] Open an [issue](https://github.com/nodejs-private/node-private) titled
  `Next Security Release`, and put this checklist in the description.

* [ ] Get agreement on the list of vulnerabilities to be addressed:
  * _**H1 REPORT LINK**_: _**DESCRIPTION**_ (_**CVE or H1 CVE request link**_)
    * v10.x, v12.x: _**LINK to PR URL**_
  * ...

* [ ] PR release announcements in [private](https://github.com/nodejs-private/nodejs.org-private):
  * (Use previous PRs as templates. Don't forget to update the site banner and
    the date in the slug so that it will move to the top of the blog list.)
  * (Consider using a [Vulnerability Score System](https://www.first.org/cvss/calculator/3.1)
    to identify severity of each report)
  * Share the patch with the reporter when applicable.
    It will increase the fix accuracy.
  * [ ] pre-release: _**LINK TO PR**_
  * [ ] post-release: _**LINK TO PR**_
    * List vulnerabilities in order of descending severity
    * Use the "summary" feature in HackerOne to sync post-release content
      and CVE requests. Example [2038134](https://hackerone.com/bugs?subject=nodejs\&report_id=2038134)
    * Ask the HackerOne reporter if they would like to be credited on the
      security release blog page:
      ```text
      Thank you to <name> for reporting this vulnerability.
      ```

* [ ] Get agreement on the planned date for the release: _**RELEASE DATE**_

* [ ] Get release team volunteers for all affected lines:
  * v12.x: _**NAME of RELEASER(S)**_
  * ... other lines, if multiple releasers

## Announcement (one week in advance of the planned release)

* [ ] Check that all vulnerabilities are ready for release integration:
  * PRs against all affected release lines or cherry-pick clean
  * Approved
  * (optional) Approved by the reporter
    * Build and send the binary to the reporter according to its architecture
      and ask for a review. This step is important to avoid insufficient fixes
      between Security Releases.
  * Pass `make test`
  * Have CVEs
    * Use the "summary" feature in HackerOne to create a description for the
      CVE and the post release announcement.
      Example [2038134](https://hackerone.com/bugs?subject=nodejs\&report_id=2038134)
    * Make sure that dependent libraries have CVEs for their issues. We should
      only create CVEs for vulnerabilities in Node.js itself. This is to avoid
      having duplicate CVEs for the same vulnerability.
  * Described in the pre/post announcements

* [ ] Pre-release announcement to nodejs.org blog: _**LINK TO BLOG**_
  (Re-PR the pre-approved branch from nodejs-private/nodejs.org-private to
  nodejs/nodejs.org)

  If the security release will only contain an OpenSSL update consider
  adding the following to the pre-release announcement:

  ```text
  Since this security release will only include updates for OpenSSL, if you're using
  a Node.js version which is part of a distribution which uses a system
  installed OpenSSL, this Node.js security update might not concern you. You may
  instead need to update your system OpenSSL libraries, please check the
  security announcements for the distribution.
  ```

* [ ] Pre-release announcement [email][]: _**LINK TO EMAIL**_
  * Subject: `Node.js security updates for all active release lines, Month Year`
  * Body:
  ```text
  The Node.js project will release new versions of all supported release lines on or shortly after Day of week, Month Day of Month, Year
  For more information see: https://nodejs.org/en/blog/vulnerability/month-year-security-releases/
  ```
  (Get access from existing manager: Matteo Collina, Rodd Vagg, Michael Dawson,
  Bryan English)

* [ ] CC `oss-security@lists.openwall.com` on pre-release

The google groups UI does not support adding a CC, until we figure
out a better way, forward the email you receive to
`oss-security@lists.openwall.com` as a CC.

* [ ] Post in the [nodejs-social channel][]
  in the OpenJS slack asking for amplication of the blog post.

  ```text
  Security release pre-alert:

  We will release new versions of <add versions> release lines on or shortly
  after Day Month Date, Year in order to address:

  - # high severity issues
  - # moderate severity issues

  https://nodejs.org/en/blog/vulnerability/month-year-security-releases/
  ```

  We specifically ask that collaborators other than the releasers and security
  steward working on the security release do not tweet or publicise the release
  until the tweet from the Node.js twitter handle goes out. We have often
  seen tweets sent out before the release and associated announcements are
  complete which may confuse those waiting for the release and also takes
  away from the work the releasers have put into shipping the releases.

* [ ] Request releaser(s) to start integrating the PRs to be released.

* [ ] Notify [docker-node][] of upcoming security release date: _**LINK**_
  ```text
  Heads up of Node.js security releases Day Month Year

  As per the Node.js security release process this is the FYI that there is going to be a security release Day Month Year
  ```

* [ ] Notify build-wg of upcoming security release date by opening an issue
  in [nodejs/build][] to request WG members are available to fix any CI issues.
  ```text
  Heads up of Node.js security releases Day Month Year

  As per security release process this is a heads up that there will be security releases Day Month Year and we'll need people from build to lock/unlock ci and to support and build issues we see.
  ```

## Release day

* [ ] [Lock CI](https://github.com/nodejs/build/blob/HEAD/doc/jenkins-guide.md#before-the-release)

* [ ] The releaser(s) run the release process to completion.

* [ ] [Unlock CI](https://github.com/nodejs/build/blob/HEAD/doc/jenkins-guide.md#after-the-release)

* [ ] Post-release announcement to Nodejs.org blog: _**LINK TO BLOG POST**_
  * (Re-PR the pre-approved branch from nodejs-private/nodejs.org-private to
    nodejs/nodejs.org)

* [ ] Post-release announcement in reply [email][]: _**LINK TO EMAIL**_
  * CC: `oss-security@lists.openwall.com`
  * Subject: `Node.js security updates for all active release lines, Month Year`
  * Body:
  ```text
  The Node.js project has now released new versions of all supported release lines.
  For more information see: https://nodejs.org/en/blog/vulnerability/month-year-security-releases/
  ```

* [ ] Post in the [nodejs-social channel][]
  in the OpenJS slack asking for amplication of the blog post.
  ```text
  Security release:

  New security releases are now available for versions <add versions> of Node.js.

  https://nodejs.org/en/blog/vulnerability/month-year-security-releases/
  ```

* [ ] Comment in [docker-node][] issue that release is ready for integration.
  The docker-node team will build and release docker image updates.

* [ ] For every H1 report resolved:
  * Close as Resolved
  * Request Disclosure
  * Request publication of [H1 CVE requests][]
    * (Check that the "Version Fixed" field in the CVE is correct, and provide
      links to the release blogs in the "Public Reference" section)

* [ ] PR machine-readable JSON descriptions of the vulnerabilities to the
  [core](https://github.com/nodejs/security-wg/tree/HEAD/vuln/core)
  vulnerability DB. _**LINK TO PR**_
  * For each vulnerability add a `#.json` file, one can copy an existing
    [json](https://github.com/nodejs/security-wg/blob/0d82062d917cb9ddab88f910559469b2b13812bf/vuln/core/78.json)
    file, and increment the latest created file number and use that as the name
    of the new file to be added. For example, `79.json`.

* [ ] Close this issue

* [ ] Make sure the PRs for the vulnerabilities are closed.

* [ ] PR in that you stewarded the release in
  [Security release stewards](https://github.com/nodejs/node/blob/HEAD/doc/contributing/security-release-process.md#security-release-stewards).
  If necessary add the next rotation of the steward rotation.

## When things go wrong

### Incomplete fixes

When a CVE is reported as fixed in a security release and it turns out that the
fix was incomplete, a new CVE should be used to cover subsequent fix. This
is best practice and avoids confusion that might occur if people believe
they have patched the original CVE by updating their Node.js version and
then we later change the `fixed in` value for the CVE.

### Updating CVEs

The steps to correct CVE information are:

* Go to the “CVE IDs” section in your program
  sections (<https://hackerone.com/nodejs/cve_requests>)
* Click the “Request a CVE ID” button
* Enter the CVE ID that needs to be updated
* Include all the details that need updating within the form
* Submit the request

[H1 CVE requests]: https://hackerone.com/nodejs/cve_requests
[docker-node]: https://github.com/nodejs/docker-node/issues
[email]: https://groups.google.com/forum/#!forum/nodejs-sec
[nodejs-social channel]: https://openjs-foundation.slack.com/archives/C0142A39BNE
[nodejs/build]: https://github.com/nodejs/build/issues
