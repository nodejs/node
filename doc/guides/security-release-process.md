# Security Release Process

The security release process covers the steps required to plan/implement a
security release. This document is copied into the description of the Next
Security Release, and used to track progess on the release. It contains ***TEXT
LIKE THIS*** which will be replaced during the release process with the
information described.

## Planning

* [ ] Open an [issue](https://github.com/nodejs-private/node-private) titled
  `Next Security Release`, and put this checklist in the description.

* [ ] Get agreement on the list of vulnerabilities to be addressed:
  * ***H1 REPORT LINK***: ***DESCRIPTION*** (***CVE or H1 CVE request link***)
    * v10.x, v12.x: ***LINK to PR URL***
  * ...

* [ ] PR release announcements in [private](https://github.com/nodejs-private/nodejs.org-private):
  * (Use previous PRs as templates, don't forget to update the site banner, and
    the date in the slug so that it will move to the top of the blog list.)
  * [ ] pre-release: ***LINK TO PR***
  * [ ] post-release: ***LINK TO PR***

* [ ] Get agreement on the planned date for the release: ***RELEASE DATE***

* [ ] Get release team volunteers for all affected lines:
  * v12.x: ***NAME of RELEASER(S)***
  * ... other lines, if multiple releasers

## Announcement (one week in advance of the planned release)

* [ ] Check that all vulnerabilities are ready for release integration:
  * PRs against all affected release lines or cherry-pick clean
  * Approved
  * Pass `make test`
  * Have CVEs
  * Described in the pre/post announcements

* [ ] Pre-release announcement [email][]: ***LINK TO EMAIL***
  (Get access from existing manager: Ben Noordhuis, Rod Vagg, Michael Dawson)

* [ ] Pre-release announcement to nodejs.org blog: ***LINK TO BLOG***
  (Re-PR the pre-approved branch from nodejs-private/nodejs.org-private to
  nodejs/nodejs.org)

* [ ] Request releaser(s) to start integrating the PRs to be released.

* [ ] Notify [docker-node][] of upcoming security release date: ***LINK***

* [ ] Notify build-wg of upcoming security release date by opening an issue
  in [nodejs/build][] to request WG members are available to fix any CI issues.

## Release day

* [ ] [Lock CI](https://github.com/nodejs/build/blob/master/doc/jenkins-guide.md#before-the-release)

* [ ] The releaser(s) run the release process to completion.

* [ ] [Unlock CI](https://github.com/nodejs/build/blob/master/doc/jenkins-guide.md#after-the-release)

* [ ] Post-release announcement in reply [email][]: ***LINK TO EMAIL***

* [ ] Post-release announcement to Nodejs.org blog: ***LINK TO BLOG POST***
  * (Re-PR the pre-approved branch from nodejs-private/nodejs.org-private to
    nodejs/nodejs.org)

* [ ] Email `"Rachel Romoff" <rromoff@linuxfoundation.org>` to tweet an
  announcement, or if you are on twitter you can just direct message the
  `@nodejs` handle.

* [ ] Comment in [docker-node][] issue that release is ready for integration.
  The docker-node team will build and release docker image updates.

* [ ] For every H1 report resolved:
  * Close as Resolved
  * Request Disclosure
  * Request publication of [H1 CVE requests][]
    * (Check that the "Version Fixed" field in the CVE is correct, and provide
      links to the release blogs in the "Public Reference" section)

* [ ] PR machine-readable JSON descriptions of the vulnerabilities to the
  [core](https://github.com/nodejs/security-wg/tree/master/vuln/core)
  vulnerability DB. ***LINK TO PR***

* [ ] Close this issue

* [ ] Make sure the PRs for the vulnerabilities are closed.

[H1 CVE requests]: https://hackerone.com/nodejs/cve_requests
[docker-node]: https://github.com/nodejs/docker-node/issues)
[nodejs/build]: https://github.com/nodejs/build/issues)
[email]: https://groups.google.com/forum/#!forum/nodejs-sec
