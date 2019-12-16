# Security Release Process

The security release process covers the steps required to plan/implement a
security release. This document is copied into the description of the Next
Security Release, and used to track progess on the release. It contains
***TEXT LIKE THIS*** which will be replaced during the release process with
the information described.

## Planning

* [ ] Open an issue in the private security repo titled `Next Security Release`
  and add this planning checklist to the description.

* [ ] Get agreement on the list of vulnerabilities to be addressed:
  * ***LINKS TO VULNS...***

* [ ] Get agreement on the planned date for the release: ***RELEASE DATE***

* [ ] Validate that all vulnerabilities have been assigned a CVE. Upstream deps
  such as OpenSSL and NPM will have CVEs, issues reported on H1 may have CVEs,
  otherwise allocate them by following the
  [cve_management_process](https://github.com/nodejs/node/blob/master/doc/guides/cve_management_process.md).

* [ ] Co-ordinate with the Release team members to line up one or more releasers
  to do the releases on the agreed date. Releaser: ***NAME of RELEASER(S)***

* [ ] Prep for the security announcements by getting agreement on drafts (use
  previously announced releases as the template):
  * pre-release: ***LINK TO COMMENT ON THIS ISSUE CONTAINING DRAFT***
  * post-release: ***LINK TO COMMENT ON THIS ISSUE CONTAINING DRAFT***

## Announcement (one week in advance of the planned release)

* Send pre-release announcement to
  https://groups.google.com/forum/#!forum/nodejs-sec.
  One of the existing managers can give access (Ben
  Noordhuis, Rod Vagg, Michael Dawson). ***LINK TO EMAIL***

* Post pre-release announcement in vulnerabilities section of Nodejs.org blog
  (https://github.com/nodejs/nodejs.org/tree/master/locale/en/blog/vulnerability).
  Use last pre-release announcement as a template (it includes blog metadata
  such as updates to the banner on the Node.js website to indicate security
  releases are coming).  Submit PR and leave 1 hour for review. After one hour
  even if no reviews, land anyway so that we don't have too big a gap between
  post to nodejs-sec and blog. Text was already reviewed in security repo so is
  unlikely to attract much additional comment. ***LINK TO BLOG PR AND POST***

* [ ] Open an issue in the build working repository with a notification of the
  date for the security release.  Use this issue to co-ordinate with the build
  team to ensure there will be coverage/availability of build team resources the
  day of the release. Those who volunteer from the build WG should be available
  in node/build during the release in case they are needed by the individual
  doing the release. ***LINK TO BUILD ISSUE***

## Release day

* [ ] The releaser(s) run the release process to completion.

* [ ] Send post-release announcement as a reply to the
  original message in https://groups.google.com/forum/#!forum/nodejs-sec
  ***LINK TO EMAIL***

* [ ] Update the blog post in
  https://github.com/nodejs/nodejs.org/tree/master/locale/en/blog/vulnerability
  with the information that releases are available and the full
  vulnerability details. Keep the original blog content at the
  bottom of the blog. Use this as an example:
  https://github.com/nodejs/nodejs.org/blob/master/locale/en/blog/vulnerability/june-2016-security-releases.md.
  Make sure to update the date in the slug so that it will move to
  the top of the blog list, and not that it updates the
  banner on Node.js org to indicate the security release(s) are
  available. ***LINK TO PR***

  *Note*: If the release blog obviously points to the people having caused the
  issue (for example when explicitly mentioning reverting a commit), adding
  those people as a CC on the PR for the blog post to give them a heads up
  might be appropriate.

* [ ] Email foundation contact to tweet out nodejs-sec announcement from
  foundation twitter account. FIXME - who is this contact?

* [ ] Create a PR to update the Node.js version in the official docker images.
  * Checkout the docker-node repo.
  * Run the update.sh using the `-s` option so that ONLY the Node.js
    versions are updated. At the request from docker (and because
    it is good practice) we limit the changes to those necessary in
    security updates.
  * Open a PR and get volunteer lined up earlier to approve.
  * Merge the PR with the merge button.
  * Checkout the [official-images](https://github.com/docker-library/official-images)
    repository .
  * In the docker-node repository run the
    [generate-stackbrew-library.sh]( https://github.com/nodejs/docker-node/blob/master/generate-stackbrew-library.sh)
    script and replace official-images/library/node with the output generated.
    ```console
    $ ./generate-stackbrew-library.sh > .../official-images/library/node
    ```
  * Open a PR with the changes to official-images/library/node making sure to
    @mention the official images.
    [maintainers](https://github.com/docker-library/official-images/blob/master/MAINTAINERS).
    In addition, make sure to prefix the PR title with `[security]`.
  * Send an email to the
    [maintainers](https://github.com/docker-library/official-images/blob/master/MAINTAINERS)
    indicating that the PR is open.

* [ ] If we allocated CVES:
  * [ ] Ensure that the announced CVEs are reported to Mitre as per the
    [cve_management_process](https://github.com/nodejs/security-wg/blob/master/processes/cve_management_process.md).
  * [ ] Ensure that the announced CVEs are updated in the cve-management
    repository as per the
    [cve_management_process](https://github.com/nodejs/security-wg/blob/master/processes/cve_management_process.md)
    so that they are listed under Announced.

* [ ] PR machine-readable JSON descriptions of the vulnerabilities to the
  [core](https://github.com/nodejs/security-wg/tree/master/vuln/core)
  vulnerability DB. ***LINK TO PR***

* [ ] Make sure the PRs for the vulnerabilities are closed.

* [ ] Ensure this issue in the private security repo for the release is closed
  out.
