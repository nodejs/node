# Node.js Core Working Groups

Node.js Core Working Groups are autonomous projects created by the
[Core Technical Committee (CTC)](https://github.com/nodejs/node/blob/master/GOVERNANCE.md#core-technical-committee).

Working Groups can be formed at any time but must be ratified by the CTC.
Once formed the work defined in the Working Group charter is the
responsibility of the WG rather than the CTC.

It is important that Working Groups are not formed pre-maturely. Working
Groups are not formed to *begin* a set of tasks but instead are formed
once that work is already underway and the contributors
think it would benefit from being done as an autonomous project.

If the work defined in a Working Group's charter is complete, the charter
should be revoked.

A Working Group's charter can be revoked either by consensus of the Working
Group's members or by a CTC vote. Once revoked, any future work that arises
becomes the responsibility of the CTC.

## Current Working Groups

* [Website](#website)
* [Streams](#streams)
* [Build](#build)
* [Diagnostics](#diagnostics)
* [i18n](#i18n)
* [Evangelism](#evangelism)
* [Docker](#docker)
* [Addon API](#addon-api)
* [Benchmarking](#benchmarking)
* [Post-mortem](#post-mortem)
* [Intl](#intl)
* [HTTP](#http)
* [Documentation](#documentation)
* [Testing](#testing)

#### Process:

* [Starting a Working Group](#starting-a-wg)
* [Bootstrap Governance](#bootstrap-governance)

### [Website](https://github.com/nodejs/nodejs.org)

The website Working Group's purpose is to build and maintain a public
website for the Node.js project.

Responsibilities include:
* Developing and maintaining a build and automation system for nodejs.org.
* Ensuring the site is regularly updated with changes made to Node.js, like
  releases and features.
* Fostering and enabling a community of translators.

### [Streams](https://github.com/nodejs/readable-stream)

The Streams Working Group is dedicated to the support and improvement of the
Streams API as used in Node.js and the npm ecosystem. We seek to create a
composable API that solves the problem of representing multiple occurrences
of an event over time in a humane, low-overhead fashion. Improvements to the
API will be driven by the needs of the ecosystem; interoperability and
backwards compatibility with other solutions and prior versions are paramount
in importance.

Responsibilities include:
* Addressing stream issues on the Node.js issue tracker.
* Authoring and editing stream documentation within the Node.js project.
* Reviewing changes to stream subclasses within the Node.js project.
* Redirecting changes to streams from the Node.js project to this project.
* Assisting in the implementation of stream providers within Node.js.
* Recommending versions of `readable-stream` to be included in Node.js.
* Messaging about the future of streams to give the community advance notice of
  changes.

### [Build](https://github.com/nodejs/build)

The Build Working Group's purpose is to create and maintain a distributed
automation infrastructure.

Responsibilities include:
* Producing packages for all target platforms.
* Running tests.
* Running performance testing and comparisons.
* Creating and managing build-containers.

### [Diagnostics](https://github.com/nodejs/diagnostics)

The Diagnostics Working Group's purpose is to surface a set of comprehensive,
documented, and extensible diagnostic interfaces for use by Node.js tools and
JavaScript VMs.

Responsibilities include:
* Collaborating with V8 to integrate `v8_inspector` into Node.js.
* Collaborating with V8 to integrate `trace_event` into Node.js.
* Collaborating with Core to refine `async_wrap` and `async_hooks`.
* Maintaining and improving OS trace system integration (e.g. ETW, LTTNG, dtrace).
* Documenting diagnostic capabilities and APIs in Node.js and its components.
* Exploring opportunities and gaps, discussing feature requests, and addressing
  conflicts in Node.js diagnostics.
* Fostering an ecosystem of diagnostics tools for Node.js.

### i18n

The i18n Working Groups handle more than just translations. They
are endpoints for community members to collaborate with each
other in their language of choice.

Each team is organized around a common spoken language. Each
language community might then produce multiple localizations for
various project resources.

Responsibilities include:
* Translating any Node.js materials they believe are relevant to their
  community.
* Reviewing processes for keeping translations up to date and of high quality.
* Managing and monitoring social media channels in their language.
* Promoting Node.js speakers for meetups and conferences in their language.

Note that the i18n Working Groups are distinct from the [Intl](#Intl) Working Group.

Each language community maintains its own membership.

* [nodejs-ar - Arabic (اللغة العربية)](https://github.com/nodejs/nodejs-ar)
* [nodejs-bg - Bulgarian (български език)](https://github.com/nodejs/nodejs-bg)
* [nodejs-bn - Bengali (বাংলা)](https://github.com/nodejs/nodejs-bn)
* [nodejs-zh-CN - Chinese (中文)](https://github.com/nodejs/nodejs-zh-CN)
* [nodejs-cs - Czech (Český Jazyk)](https://github.com/nodejs/nodejs-cs)
* [nodejs-da - Danish (Dansk)](https://github.com/nodejs/nodejs-da)
* [nodejs-de - German (Deutsch)](https://github.com/nodejs/nodejs-de)
* [nodejs-el - Greek (Ελληνικά)](https://github.com/nodejs/nodejs-el)
* [nodejs-es - Spanish (Español)](https://github.com/nodejs/nodejs-es)
* [nodejs-fa - Persian (فارسی)](https://github.com/nodejs/nodejs-fa)
* [nodejs-fi - Finnish (Suomi)](https://github.com/nodejs/nodejs-fi)
* [nodejs-fr - French (Français)](https://github.com/nodejs/nodejs-fr)
* [nodejs-he - Hebrew (עברית)](https://github.com/nodejs/nodejs-he)
* [nodejs-hi - Hindi (फिजी बात)](https://github.com/nodejs/nodejs-hi)
* [nodejs-hu - Hungarian (Magyar)](https://github.com/nodejs/nodejs-hu)
* [nodejs-id - Indonesian (Bahasa Indonesia)](https://github.com/nodejs/nodejs-id)
* [nodejs-it - Italian (Italiano)](https://github.com/nodejs/nodejs-it)
* [nodejs-ja - Japanese (日本語)](https://github.com/nodejs/nodejs-ja)
* [nodejs-ka - Georgian (ქართული)](https://github.com/nodejs/nodejs-ka)
* [nodejs-ko - Korean (조선말)](https://github.com/nodejs/nodejs-ko)
* [nodejs-mk - Macedonian (Mакедонски)](https://github.com/nodejs/nodejs-mk)
* [nodejs-ms - Malay (بهاس ملايو)](https://github.com/nodejs/nodejs-ms)
* [nodejs-nl - Dutch (Nederlands)](https://github.com/nodejs/nodejs-nl)
* [nodejs-no - Norwegian (Norsk)](https://github.com/nodejs/nodejs-no)
* [nodejs-pl - Polish (Język Polski)](https://github.com/nodejs/nodejs-pl)
* [nodejs-pt - Portuguese (Português)](https://github.com/nodejs/nodejs-pt)
* [nodejs-ro - Romanian (Română)](https://github.com/nodejs/nodejs-ro)
* [nodejs-ru - Russian (Русский)](https://github.com/nodejs/nodejs-ru)
* [nodejs-sv - Swedish (Svenska)](https://github.com/nodejs/nodejs-sv)
* [nodejs-ta - Tamil (தமிழ்)](https://github.com/nodejs/nodejs-ta)
* [nodejs-tr - Turkish (Türkçe)](https://github.com/nodejs/nodejs-tr)
* [nodejs-zh-TW - Taiwanese (Hō-ló)](https://github.com/nodejs/nodejs-zh-TW)
* [nodejs-uk - Ukrainian (Українська)](https://github.com/nodejs/nodejs-uk)
* [nodejs-vi - Vietnamese (Tiếng Việtnam)](https://github.com/nodejs/nodejs-vi)

### [Intl](https://github.com/nodejs/Intl)

The Intl Working Group is dedicated to support and improvement of
Internationalization (i18n) and Localization (l10n) in Node.

Responsibilities include:
* Ensuring functionality & compliance (standards: ECMA, Unicode…)
* Supporting Globalization and Internationalization issues that come up
  in the tracker
* Communicating guidance and best practices
* Refining the existing `Intl` implementation

The Intl Working Group is not responsible for translation of content. That is the
responsibility of the specific [i18n](#i18n) group for each language.

### [Evangelism](https://github.com/nodejs/evangelism)

The Evangelism Working Group promotes the accomplishments
of Node.js and lets the community know how they can get involved.

Responsibilities include:
* Facilitating project messaging.
* Managing official project social media.
* Handling the promotion of speakers for meetups and conferences.
* Handling the promotion of community events.
* Publishing regular update summaries and other promotional
  content.

### [HTTP](https://github.com/nodejs/http)

The HTTP Working Group is chartered for the support and improvement of the
HTTP implementation in Node.js.

Responsibilities include:
* Addressing HTTP issues on the Node.js issue tracker.
* Authoring and editing HTTP documentation within the Node.js project.
* Reviewing changes to HTTP functionality within the Node.js project.
* Working with the ecosystem of HTTP related module developers to evolve the
  HTTP implementation and APIs in core.
* Advising the CTC on all HTTP related issues and discussions.
* Messaging about the future of HTTP to give the community advance notice of
  changes.

### [Docker](https://github.com/nodejs/docker-iojs)

The Docker Working Group's purpose is to build, maintain, and improve official
Docker images for the Node.js project.

Responsibilities include:
* Keeping the official Docker images updated in line with new Node.js releases.
* Decide and implement image improvements and/or fixes.
* Maintain and improve the images' documentation.

### [Addon API](https://github.com/nodejs/nan)

The Addon API Working Group is responsible for maintaining the NAN project and
corresponding _nan_ package in npm. The NAN project makes available an
abstraction layer for native add-on authors for Node.js,
assisting in the writing of code that is compatible with many actively used
versions of Node.js, V8 and libuv.

Responsibilities include:
* Maintaining the [NAN](https://github.com/nodejs/nan) GitHub repository,
  including code, issues and documentation.
* Maintaining the [addon-examples](https://github.com/nodejs/node-addon-examples)
  GitHub repository, including code, issues and documentation.
* Maintaining the C++ Addon API within the Node.js project, in subordination to
  the Node.js CTC.
* Maintaining the Addon documentation within the Node.js project, in
  subordination to the Node.js CTC.
* Maintaining the _nan_ package in npm, releasing new versions as appropriate.
* Messaging about the future of the Node.js and NAN interface to give the
  community advance notice of changes.

The current members can be found in their
[README](https://github.com/nodejs/nan#collaborators).

### [Benchmarking](https://github.com/nodejs/benchmarking)

The purpose of the Benchmark Working Group is to gain consensus
on an agreed set of benchmarks that can be used to:

* track and evangelize performance gains made between Node.js releases
* avoid performance regressions between releases

Responsibilities include:
* Identifying 1 or more benchmarks that reflect customer usage.
  Likely will need more than one to cover typical Node.js use cases
  including low-latency and high concurrency
* Working to get community consensus on the list chosen
* Adding regular execution of chosen benchmarks to Node.js builds
* Tracking/publicizing performance between builds/releases

### [Post-mortem](https://github.com/nodejs/post-mortem)

The Post-mortem Diagnostics Working Group is dedicated to the support
and improvement of postmortem debugging for Node.js. It seeks to
elevate the role of postmortem debugging for Node, to assist in the
development of techniques and tools, and to make techniques and tools
known and available to Node.js users.

Responsibilities include:
* Defining and adding interfaces/APIs in order to allow dumps
  to be generated when needed.
* Defining and adding common structures to the dumps generated
  in order to support tools that want to introspect those dumps.

### [Documentation](https://github.com/nodejs/docs)

The Documentation Working Group exists to support the improvement of Node.js
documentation, both in the core API documentation, and elsewhere, such as the
Node.js website. Its intent is to work closely with the Evangelism, Website, and
Intl Working Groups to make excellent documentation available and accessible
to all.

Responsibilities include:
* Defining and maintaining documentation style and content standards.
* Producing documentation in a format acceptable for the Website Working Group
  to consume.
* Ensuring that Node's documentation addresses a wide variety of audiences.
* Creating and operating a process for documentation review that produces
  quality documentation and avoids impeding the progress of Core work.

### [Testing](https://github.com/nodejs/testing)

The Node.js Testing Working Group's purpose is to extend and improve testing of
the Node.js source code.

Responsibilities include:
* Coordinating an overall strategy for improving testing.
* Documenting guidelines around tests.
* Working with the Build Working Group to improve continuous integration.
* Improving tooling for testing.

## Joining a WG

To find out how to join a working group, consult the GOVERNANCE.md in
the working group's repository, or simply open an issue there.

## Starting a WG

A Working Group is established by first defining a charter  that can be
ratified by the TC. A charter is a *statement of purpose*, a
*list of responsibilities* and a *list of initial membership*.

A working group needs 3 initial members. These should be individuals
already undertaking the work described in the charter.

The list of responsibilities should be specific. Once established, these
responsibilities are no longer governed by the TC and therefore should
not be broad or subjective. The only recourse the TC has over the working
group is to revoke the entire charter and take on the work previously
done by the working group themselves.

If the responsibilities described in the charter are currently
undertaken by another WG then the charter will additionally have to be
ratified by that WG.

You can submit the WG charter for ratification by sending
a Pull Request to this document, which adds it to the
list of current Working Groups. Once ratified the list of
members should be maintained in the Working Group's
README.

## Bootstrap Governance

Once the TC ratifies a charter the WG inherits the following
documentation for governance, contribution, conduct and an MIT
LICENSE. The WG is free to change these documents through their own
governance process, hence the term "bootstrap."

### *[insert WG name]* Working Group

The Node.js *[insert WG name]* is jointly governed by a Working Group (WG)
that is responsible for high-level guidance of the project.

The WG has final authority over this project including:

* Technical direction
* Project governance and process (including this policy)
* Contribution policy
* GitHub repository hosting
* Conduct guidelines
* Maintaining the list of additional Collaborators

For the current list of WG members, see the project
[README.md](./README.md#current-project-team-members).

### Collaborators

The *[insert WG name]* GitHub repository is
maintained by the WG and additional Collaborators who are added by the
WG on an ongoing basis.

Individuals making significant and valuable contributions are made
Collaborators and given commit-access to the project. These
individuals are identified by the WG and their addition as
Collaborators is discussed during the weekly WG meeting.

_Note:_ If you make a significant contribution and are not considered
for commit-access log an issue or contact a WG member directly and it
will be brought up in the next WG meeting.

Modifications of the contents of the *[insert WG repo]* repository are made on
a collaborative basis. Anybody with a GitHub account may propose a
modification via pull request and it will be considered by the project
Collaborators. All pull requests must be reviewed and accepted by a
Collaborator with sufficient expertise who is able to take full
responsibility for the change. In the case of pull requests proposed
by an existing Collaborator, an additional Collaborator is required
for sign-off. Consensus should be sought if additional Collaborators
participate and there is disagreement around a particular
modification. See _Consensus Seeking Process_ below for further detail
on the consensus model used for governance.

Collaborators may opt to elevate significant or controversial
modifications, or modifications that have not found consensus to the
WG for discussion by assigning the ***WG-agenda*** tag to a pull
request or issue. The WG should serve as the final arbiter where
required.

For the current list of Collaborators, see the project
[README.md](./README.md#current-project-team-members).

### WG Membership

WG seats are not time-limited.  There is no fixed size of the WG.
However, the expected target is between 6 and 12, to ensure adequate
coverage of important areas of expertise, balanced with the ability to
make decisions efficiently.

There is no specific set of requirements or qualifications for WG
membership beyond these rules.

The WG may add additional members to the WG by unanimous consensus.

A WG member may be removed from the WG by voluntary resignation, or by
unanimous consensus of all other WG members.

Changes to WG membership should be posted in the agenda, and may be
suggested as any other agenda item (see "WG Meetings" below).

If an addition or removal is proposed during a meeting, and the full
WG is not in attendance to participate, then the addition or removal
is added to the agenda for the subsequent meeting.  This is to ensure
that all members are given the opportunity to participate in all
membership decisions.  If a WG member is unable to attend a meeting
where a planned membership decision is being made, then their consent
is assumed.

No more than 1/3 of the WG members may be affiliated with the same
employer.  If removal or resignation of a WG member, or a change of
employment by a WG member, creates a situation where more than 1/3 of
the WG membership shares an employer, then the situation must be
immediately remedied by the resignation or removal of one or more WG
members affiliated with the over-represented employer(s).

### WG Meetings

The WG meets weekly on a Google Hangout On Air. A designated moderator
approved by the WG runs the meeting. Each meeting should be
published to YouTube.

Items are added to the WG agenda that are considered contentious or
are modifications of governance, contribution policy, WG membership,
or release process.

The intention of the agenda is not to approve or review all patches;
that should happen continuously on GitHub and be handled by the larger
group of Collaborators.

Any community member or contributor can ask that something be added to
the next meeting's agenda by logging a GitHub Issue. Any Collaborator,
WG member or the moderator can add the item to the agenda by adding
the ***WG-agenda*** tag to the issue.

Prior to each WG meeting the moderator will share the Agenda with
members of the WG. WG members can add any items they like to the
agenda at the beginning of each meeting. The moderator and the WG
cannot veto or remove items.

The WG may invite persons or representatives from certain projects to
participate in a non-voting capacity.

The moderator is responsible for summarizing the discussion of each
agenda item and sends it as a pull request after the meeting.

### Consensus Seeking Process

The WG follows a
[Consensus Seeking](http://en.wikipedia.org/wiki/Consensus-seeking_decision-making)
decision-making model.

When an agenda item has appeared to reach a consensus the moderator
will ask "Does anyone object?" as a final call for dissent from the
consensus.

If an agenda item cannot reach a consensus a WG member can call for
either a closing vote or a vote to table the issue to the next
meeting. The call for a vote must be seconded by a majority of the WG
or else the discussion will continue. Simple majority wins.

Note that changes to WG membership require unanimous consensus.  See
"WG Membership" above.

<a id="developers-certificate-of-origin"></a>
## Developer's Certificate of Origin 1.1

By making a contribution to this project, I certify that:

* (a) The contribution was created in whole or in part by me and I
  have the right to submit it under the open source license
  indicated in the file; or

* (b) The contribution is based upon previous work that, to the best
  of my knowledge, is covered under an appropriate open source
  license and I have the right under that license to submit that
  work with modifications, whether created in whole or in part
  by me, under the same open source license (unless I am
  permitted to submit under a different license), as indicated
  in the file; or

* (c) The contribution was provided directly to me by some other
  person who certified (a), (b) or (c) and I have not modified
  it.

* (d) I understand and agree that this project and the contribution
  are public and that a record of the contribution (including all
  personal information I submit with it, including my sign-off) is
  maintained indefinitely and may be redistributed consistent with
  this project or the open source license(s) involved.

### Moderation Policy

The [Node.js Moderation Policy] applies to this WG.

### Code of Conduct

The [Node.js Code of Conduct][] applies to this WG.

[Node.js Code of Conduct]: https://github.com/nodejs/node/blob/master/CODE_OF_CONDUCT.md
[Node.js Moderation Policy]: https://github.com/nodejs/TSC/blob/master/Moderation-Policy.md
