# Security release process

The security release process covers the steps required to plan/implement a
security release. This document is copied into the description of the Next
Security Release and used to track progress on the release. It contains ***TEXT
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
  * (Use previous PRs as templates. Don't forget to update the site banner and
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
   * CC: `oss-security@lists.openwall.com`
   * Subject: `Node.js security updates for all active release lines, Month Year`
   * Body:
  ```text
  The Node.js project will release new versions of all supported release lines on or shortly after Day of week, Month Day of Month, Year
  For more information see: https://nodejs.org/en/blog/vulnerability/month-year-security-releases/
  ```
  (Get access from existing manager: Ben Noordhuis, Rod Vagg, Michael Dawson)

* [ ] Pre-release announcement to nodejs.org blog: ***LINK TO BLOG***
  (Re-PR the pre-approved branch from nodejs-private/nodejs.org-private to
  nodejs/nodejs.org)

* [ ] Post in the #nodejs-social channel in the OpenJS Foundation Slack
  asking that the social team tweet/retweet the pre-announcement.
  If you are on Twitter, you can just direct message the `@nodejs` handle.

* [ ] Request releaser(s) to start integrating the PRs to be released.

* [ ] Notify [docker-node][] of upcoming security release date: ***LINK***
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

* [ ] Post-release announcement in reply [email][]: ***LINK TO EMAIL***
   * CC: `oss-security@lists.openwall.com`
   * Subject: `Node.js security updates for all active release lines, Month Year`
   * Body:
  ```text
  The Node.js project has now released new versions of all supported release lines.
  For more information see: https://nodejs.org/en/blog/vulnerability/month-year-security-releases/
  ```

* [ ] Post-release announcement to Nodejs.org blog: ***LINK TO BLOG POST***
  * (Re-PR the pre-approved branch from nodejs-private/nodejs.org-private to
    nodejs/nodejs.org)

* [ ] Post in the #nodejs-social channel in the OpenJS Foundation Slack
  asking that the social team tweet/retweet the announcement.
  If you are on Twitter, you can just direct message the `@nodejs` handle.

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
  vulnerability DB. ***LINK TO PR***
  * For each vulnerability add a `#.json` file, one can copy an existing
    [json](https://github.com/nodejs/security-wg/blob/0d82062d917cb9ddab88f910559469b2b13812bf/vuln/core/78.json)
    file, and increment the latest created file number and use that as the name
    of the new file to be added. For example, `79.json`.

* [ ] Close this issue

* [ ] Make sure the PRs for the vulnerabilities are closed.

[H1 CVE requests]: https://hackerone.com/nodejs/cve_requests
[docker-node]: https://github.com/nodejs/docker-node/issues
[email]: https://groups.google.com/forum/#!forum/nodejs-sec
[nodejs/build]: https://github.com/nodejs/build/issues
