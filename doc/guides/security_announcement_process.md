The Node.js community follows a process to create/review and
then publish vulnerability announcements. It is most often a 2 step
process where we:

* announce that releases will be made to fix an embargoed vulnerability
* announce that the releases with the fixes are available

The process is as follows:

* Security vulnerabilties are initially discussed/reviewed in the private
  security repository.

* Once we are ready to release an anouncement of an upcoming fix for the
  the vulnerability, on the issue for the security vulnerability in private
  security repo, propose candidate text based on past announcements.

* Once reviewed, agree on timing for the releases with the fix and line up
  releasers to make sure they are available to do the release on that date.

* Post to https://groups.google.com/forum/#!forum/nodejs-sec.
  **Note** that you will need to have been given access by one of the
  existing managers (Ben Noordhuis, Rod Vagg, Trevor Norris, Michael Dawson).
  You will have to manually edit to add formatting and links properly.

* Mirror post in vulnerabilities section of Nodejs.org blog section
  (https://github.com/nodejs/nodejs.org/tree/master/locale/en/blog/vulnerability)
  Submit PR and leave 1 hour for review. After one hour even if no reviews,
  land anyway so that we don't have too big a gap between post to nodejs-sec
  and blog. Text was already reviewed in security repo so is unlikely to
  attract much additional comment. **The PR should also update the banner
  on the Node.js website to indicate security releases are coming with the
  banner linked to the blog**

* In original PR for the security repository for the issue, post candidate
  text for updates that will go out with releases that will indicates
  releases are available and include full vulnerability details.

* Once releases are made, post response to original message in
  https://groups.google.com/forum/#!forum/nodejs-sec indicating
  releases are available and with the full vulnerability details.

* Update the blog post in
  https://github.com/nodejs/nodejs.org/tree/master/locale/en/blog/vulnerability
  with the information that releases are available and the full
  vulnerability details. Keep the original blog content at the
  bottom of the blog. This is an example:
  https://github.com/nodejs/nodejs.org/blob/master/locale/en/blog/vulnerability/june-2016-security-releases.md.
  Make sure to update the date in the slug so that it will move to
  the top of the blog list. **As part of the PR, update the
  banner on Node.js org to indicate the security release are
  available.**

  *Note*: If the release blog obviously points to the people having caused the
  issue (for example when explicitly mentioning reverting a commit), adding
  those people as a CC on the PR for the blog post to give them a heads up
  might be appropriate.

* Tweet out a link to the nodejs-sec announcement.

* Email foundation contact to tweet out nodejs-sec announcement from
  foundation twitter account.
