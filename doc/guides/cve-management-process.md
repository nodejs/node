# Node.js CVE management process

The Node.js project acts as a [Common Vulnerabilities and Exposures (CVE)
Numbering Authority (CNA)](https://cve.mitre.org/cve/cna.html).
The current scope is for all actively developed versions of software
developed under the Node.js project (ie.  https://github.com/nodejs).
This means that the Node.js team reviews CVE requests and if appropriate
assigns CVE numbers to vulnerabilities.  The scope currently **does not**
include third party modules.

More detailed information about the CNA program is available in
[CNA_Rules_v1.1](https://cve.mitre.org/cve/cna/CNA_Rules_v1.1.pdf).

## Contacts

As part of the CNA program the Node.js team must provide a number
of contact points.  Email aliases have been setup for these as follows:

* **Public contact points**. Email address to which people will be directed
  by Mitre when they are asked for a way to contact the Node.js team about
  CVE-related issues. **cve-request@iojs.org**

* **Private contact points**. Administrative contacts that Mitre can reach out
   to directly in case there are issues that require immediate attention.
   **cve-mitre-contact@iojs.org**

* **Email addresses to add to the CNA email discussion list**. This address has
   been added to a closed mailing list that is used for announcements,
   sharing documents, or discussion relevant to the CNA community.
   The list rarely has more than ten messages a week.
   **cna-discussion-list@iojs.org**

## CNA management processes

### CVE Block management

The CNA program allows the Node.js team to request a block of CVEs in
advance. These CVEs are managed in a repository within the Node.js
private organization called
[cve-management](https://github.com/nodejs-private/cve-management).
For each year there will be a markdown file titled "cve-management-XXXX"
where XXXX is the year (for example 'cve-management-2017.md').

This file will have the following sections:

* Available
* Pending
* Announced

When a new block of CVEs is received from Mitre they will be listed under
the `Available` section.

These CVEs will be moved from the Available to Pending and Announced
as outlined in the section titled `CVE Management process`.

In addition, when moving a CVE from Available such that there are less
than two remaining CVEs a new block must be requested as follows:

* Use the Mitre request form https://cveform.mitre.org/ with the
  option `Request a Block of IDs` to request a new block.
* The new block will be sent to the requester through email.
* Once the new block has been received, the requester will add them
  to the Available list.

All changes to the files for managing CVEs in a given year will
be done through Pull Requests so that we have a record of how
the CVEs have been assigned.

CVEs are only valid for a specific year.  At the beginning of each
year the old CVEs should be removed from the list. A new block
of CVEs should then be requested using the steps listed above.

### External CVE request process

When a request for a CVE is received via the cve-request@iojs.org
email alias the following process will be followed (likely updated
after we get HackerOne up and running).

* Respond to the requester indicating that we have the request
  and will review soon.
* Open an issue in the security repo for the request.
* Review the request.
  * If a CVE is appropriate then assign the
    CVE as outline in the section titled
    [CVE Management process for Node.js vulnerabilities](#cve-management-process-for-nodejs-vulnerabilities)
    and return the CVE number to the requester (along with the request
    to keep it confidential until the vulnerability is announced)
  * If a CVE is not appropriate then respond to the requester
    with the details as to why.

### Quarterly reporting

* There is a requirement for quarterly reports to Mitre on CVE
  activity.  Not sure of the specific requirements yet.  Will
  add details on process once we've done the first one.

## CVE Management process for Node.js vulnerabilities

When the Node.js team is going announce a new vulnerability the
following steps are used to assign, announce and report a CVE.

* Select the next CVE in the block available from the CNA process as
  outlined in the section above.
* Move the CVE from the unassigned block, to the Pending section along
  with a link to the issue in the security repo that is being used
  to discuss the vulnerability.
* As part of the
  [security announcement process](https://github.com/nodejs/security-wg/blob/master/processes/security_annoucement_process.md),
  in the security issue being used to discuss the
  vulnerability, associate the CVE to that vulnerability. This is most
  commonly done by including it in the draft for the announcement that
  will go out once the associated security releases are available.
* Once the security announcement goes out:
  * Use the [Mitre form](https://cveform.mitre.org/) to report the
    CVE details to Mitre using the `Notify CVE about a publication`. The
    link to the advisory will be the for the blog announcing that security
    releases are available. The description should be a subset of the
    details in that blog.

    Ensure that the contact address entered in the form is
    `cve-mitre-contact@iojs.org`. Anything else may require slow, manual
    verification of the identity of the CVE submitter.

    For each CVE listed, the additional data must include the following fields
    updated with appropriate data for the CVE
    ```text
    [CVEID]: CVE-XXXX-XXXX
    [PRODUCT]: Node.js
    [VERSION]: 8.x+, 9.x+, 10.x+
    [PROBLEMTYPE]: Denial of Service
    [REFERENCES]: Link to the blog for the final announce
    [DESCRIPTION]: Description from final announce
    [ASSIGNINGCNA]: Node.js Foundation
    ```
* Move the CVE from the Pending section to the Announced section along
  with a link to the Node.js blog post announcing that releases
  are available.
