# Security Release Process

The security release process covers the steps required to plan/implement
a security release.

## Planning

* [ ] Open an issue in the private security repo titled `Next Security Release`
  and add the planning checklist to the description.

* [ ] Get agreement on the list of vulnerabilities to be addressed.

* [ ] Get agreement on the planned date for the releases.

* [ ] Once agreement on the list and date has been agreed, validate that all
  vulnerabilities have been assigned a CVE following the
  [cve_management_process](https://github.com/nodejs/security-wg/blob/master/processes/cve_management_process.md).

* [ ] Co-ordinate with the Release team members to line up one or more releasers
  to do the releases on the agreed date.

* [ ] Prep for the pre-security announcement and final security announcement by
  getting agreement on drafts following the
  [security_announcement_process](https://github.com/nodejs/security-wg/blob/master/processes/security_annoucement_process.md).

## Announcement (one week in advance of the planned release)

* [ ] Ensure the pre-announce is sent out as outlined in the
  [security_announcement_process](https://github.com/nodejs/security-wg/blob/master/processes/security_annoucement_process.md).

* [ ] Open an issue in the build working repository with a notification of the
  date for the security release.  Use this issue to co-ordinate with the build
  team to ensure there will be coverage/availability of build team resources the
  day of the release. Those who volunteer from the build WG should be available
  in node-build during the release in case they are needed by the individual
  doing the release.

* [ ] Send an email to the docker official image
  [maintainers](https://github.com/docker-library/official-images/blob/master/MAINTAINERS)
  with an FYI that security releases will be going out on the agreed date.

* [ ] Open an issue in the [docker-node](https://github.com/nodejs/docker-node)
  repo and get one or more volunteers to be available to review the PR to update
  Node.js versions in the docker-node repo immediately after the release.

* [ ] Call on the sec release volunteer(s) to start integrating the PRs, running
  the CI jobs, and generally prepping the release.

## Release day

* [ ] Co-ordinate with the Release team members and keep up to date on progress.
  Get an guesstimate of when releases may be ready and send an FYI to the docker
  official image
  [maintainers](https://github.com/docker-library/official-images/blob/master/MAINTAINERS).

* [ ] When the releases are promoted, ensure the final announce goes out as per
  the
  [security_announcement_process](https://github.com/nodejs/security-wg/blob/master/processes/security_annoucement_process.md).

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

* [ ] Ensure that the announced CVEs are reported to Mitre as per the
  [cve_management_process](https://github.com/nodejs/security-wg/blob/master/processes/cve_management_process.md).

* [ ] Ensure that the announced CVEs are updated in the cve-management
  repository as per the
  [cve_management_process](https://github.com/nodejs/security-wg/blob/master/processes/cve_management_process.md)
  so that they are listed under Announced.

* [ ] PR machine-readable JSON descriptions of the vulnerabilities to the
  [core](https://github.com/nodejs/security-wg/tree/master/vuln/core)
  vulnerability DB.

* [ ] Make sure the PRs for the vulnerabilities are closed.

* [ ] Ensure the issue in the private security repo for the release is closed
  out.
