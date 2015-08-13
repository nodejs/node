# Node.js Working Groups

Node.js Working Groups are autonomous projects created by the
[Technical Steering Committee (TSC)](https://github.com/nodejs/node/blob/master/GOVERNANCE.md#technical-committee).

Working Groups can be formed at any time but must be ratified by the TSC.
Once formed the work defined in the Working Group charter is the
responsibility of the WG rather than the TSC.

It is important that Working Groups are not formed pre-maturely. Working
Groups are not formed to *begin* a set of tasks but instead are formed
once that work is already underway and the contributors
think it would benefit from being done as an autonomous project.

If the work defined in a Working Group charter is completed the Working
Group should be dissolved and the responsibility for governance absorbed
back in to the TSC.

## Current Working Groups

* [Website](#website)
* [Streams](#streams)
* [Build](#build)
* [Tracing](#tracing)
* [i18n](#i18n)
* [Evangelism](#evangelism)
* [Roadmap](#roadmap)
* [Docker](#docker)
* [Addon API](#addon-api)
* [Starting a Working Group](#starting-a-wg)
* [Bootstrap Governance](#bootstrap-governance)

### [Website](https://github.com/nodejs/website)

The website working group's purpose is to build and maintain a public
website for the `Node.js` project.

Its responsibilities are:
* Develop and maintain a build and automation system for `nodejs.org`.
* Ensure the site is regularly updated with changes made to `Node.js` like
releases and features.
* Foster and enable a community of translators.

### [Streams](https://github.com/nodejs/readable-stream)

The Streams WG is dedicated to the support and improvement of the Streams API
as used in Node.js and the npm ecosystem. We seek to create a composable API that
solves the problem of representing multiple occurrences of an event over time
in a humane, low-overhead fashion. Improvements to the API will be driven by
the needs of the ecosystem; interoperability and backwards compatibility with
other solutions and prior versions are paramount in importance. Our
responsibilities include:

* Addressing stream issues on the Node.js issue tracker.
* Authoring and editing stream documentation within the Node.js project.
* Reviewing changes to stream subclasses within the Node.js project.
* Redirecting changes to streams from the Node.js project to this project.
* Assisting in the implementation of stream providers within Node.js.
* Recommending versions of readable-stream to be included in Node.js.
* Messaging about the future of streams to give the community advance notice of changes.


### [Build](https://github.com/nodejs/build)

The build working group's purpose is to create and maintain a
distributed automation infrastructure.

Its responsibilities are:
* Produce Packages for all target platforms.
* Run tests.
* Run performance testing and comparisons.
* Creates and manages build-containers.


### [Tracing](https://github.com/nodejs/tracing-wg)

The tracing working group's purpose is to increase the
transparency of software written in Node.js.

Its responsibilities are:
* Collaboration with V8 to integrate with `trace_event`.
* Maintenance and iteration on AsyncWrap.
* Maintenance and improvements to system tracing support (DTrace, LTTng, etc.)
* Documentation of tracing and debugging techniques.
* Fostering a tracing and debugging ecosystem.

### i18n

The i18n working groups handle more than just translations. They
are endpoints for community members to collaborate with each
other in their language of choice.

Each team is organized around a common spoken language. Each
language community might then produce multiple localizations for
various project resources.

Their responsibilities are:
* Translations of any Node.js materials they believe are relevant to their
community.
* Review processes for keeping translations up
to date and of high quality.
* Social media channels in their language.
* Promotion of Node.js speakers for meetups and conferences in their
language.

Each language community maintains its own membership.

* [iojs-ar - Arabic (اللغة العربية)](https://github.com/nodejs/iojs-ar)
* [iojs-bg - Bulgarian (български език)](https://github.com/nodejs/iojs-bg)
* [iojs-bn - Bengali (বাংলা)](https://github.com/nodejs/iojs-bn)
* [iojs-cn - Chinese (中文)](https://github.com/nodejs/iojs-cn)
* [iojs-cs - Czech (Český Jazyk)](https://github.com/nodejs/iojs-cs)
* [iojs-da - Danish (Dansk)](https://github.com/nodejs/iojs-da)
* [iojs-de - German (Deutsch)](https://github.com/nodejs/iojs-de)
* [iojs-el - Greek (Ελληνικά)](https://github.com/nodejs/iojs-el)
* [iojs-es - Spanish (Español)](https://github.com/nodejs/iojs-es)
* [iojs-fa - Persian (فارسی)](https://github.com/nodejs/iojs-fa)
* [iojs-fi - Finnish (Suomi)](https://github.com/nodejs/iojs-fi)
* [iojs-fr - French (Français)](https://github.com/nodejs/iojs-fr)
* [iojs-he - Hebrew (עברית)](https://github.com/nodejs/iojs-he)
* [iojs-hi - Hindi (फिजी बात)](https://github.com/nodejs/iojs-hi)
* [iojs-hu - Hungarian (Magyar)](https://github.com/nodejs/iojs-hu)
* [iojs-id - Indonesian (Bahasa Indonesia)](https://github.com/nodejs/iojs-id)
* [iojs-it - Italian (Italiano)](https://github.com/nodejs/iojs-it)
* [iojs-ja - Japanese (日本語)](https://github.com/nodejs/iojs-ja)
* [iojs-ka - Georgian (ქართული)](https://github.com/nodejs/iojs-ka)
* [iojs-ko - Korean (조선말)](https://github.com/nodejs/iojs-ko)
* [iojs-mk - Macedonian (Mакедонски)](https://github.com/nodejs/iojs-mk)
* [iojs-ms - Malay (بهاس ملايو)](https://github.com/nodejs/iojs-ms)
* [iojs-nl - Dutch (Nederlands)](https://github.com/nodejs/iojs-nl)
* [iojs-no - Norwegian (Norsk)](https://github.com/nodejs/iojs-no)
* [iojs-pl - Polish (Język Polski)](https://github.com/nodejs/iojs-pl)
* [iojs-pt - Portuguese (Português)](https://github.com/nodejs/iojs-pt)
* [iojs-ro - Romanian (Română)](https://github.com/nodejs/iojs-ro)
* [iojs-ru - Russian (Русский)](https://github.com/nodejs/iojs-ru)
* [iojs-sv - Swedish (Svenska)](https://github.com/nodejs/iojs-sv)
* [iojs-ta - Tamil (தமிழ்)](https://github.com/nodejs/iojs-ta)
* [iojs-tr - Turkish (Türkçe)](https://github.com/nodejs/iojs-tr)
* [iojs-tw - Taiwanese (Hō-ló)](https://github.com/nodejs/iojs-tw)
* [iojs-uk - Ukrainian (Українська)](https://github.com/nodejs/iojs-uk)
* [iojs-vi - Vietnamese (Tiếng Việtnam)](https://github.com/nodejs/iojs-vi)


### [Evangelism](https://github.com/nodejs/evangelism)

The evangelism working group promotes the accomplishments
of Node.js and lets the community know how they can get involved.

Their responsibilities are:
* Project messaging.
* Official project social media.
* Promotion of speakers for meetups and conferences.
* Promotion of community events.
* Publishing regular update summaries and other promotional
content.


### [Roadmap](https://github.com/nodejs/roadmap)

The roadmap working group is responsible for user community outreach
and the translation of their concerns into a plan of action for Node.js.

The final [ROADMAP](./ROADMAP.md) document is still owned by the TC and requires
the same approval for changes as any other project asset.

Their responsibilities are:
* Attract and summarize user community needs and feedback.
* Find or potentially create tools that allow for broader participation.
* Create Pull Requests for relevant changes to [Roadmap.md](./ROADMAP.md)


### [Docker](https://github.com/nodejs/docker-iojs)

The Docker working group's purpose is to build, maintain, and improve official
Docker images for the `Node.js` project.

Their responsibilities are:
* Keep the official Docker images updated in line with new `Node.js` releases.
* Decide and implement image improvements and/or fixes.
* Maintain and improve the images' documentation.


### [Addon API](https://github.com/nodejs/nan)

The Addon API Working Group is responsible for maintaining the NAN project and
corresponding _nan_ package in npm. The NAN project makes available an
abstraction layer for native add-on authors for both Node.js and Node.js,
assisting in the writing of code that is compatible with many actively used
versions of Node.js, Node.js, V8 and libuv.

Their responsibilities are:

* Maintaining the [NAN](https://github.com/nodejs/nan) GitHub repository,
  including code, issues and documentation.
* Maintaining the [addon-examples](https://github.com/nodejs/node-addon-examples)
  GitHub repository, including code, issues and documentation.
* Maintaining the C++ Addon API within the Node.js project, in subordination to
  the Node.js TSC.
* Maintaining the Addon documentation within the Node.js project, in
  subordination to the Node.js TSC.
* Maintaining the _nan_ package in npm, releasing new versions as appropriate.
* Messaging about the future of the Node.js and NAN interface to give the
  community advance notice of changes.

The current members can be found in their
[README](https://github.com/nodejs/nan#collaborators).

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

### Developer's Certificate of Origin 1.0

By making a contribution to this project, I certify that:

* (a) The contribution was created in whole or in part by me and I
  have the right to submit it under the open source license indicated
  in the file; or
* (b) The contribution is based upon previous work that, to the best
  of my knowledge, is covered under an appropriate open source license
  and I have the right under that license to submit that work with
  modifications, whether created in whole or in part by me, under the
  same open source license (unless I am permitted to submit under a
  different license), as indicated in the file; or
* (c) The contribution was provided directly to me by some other
  person who certified (a), (b) or (c) and I have not modified it.


### Code of Conduct

This Code of Conduct is adapted from [Rust's wonderful
CoC](https://github.com/rust-lang/rust/wiki/Note-development-policy#conduct).

* We are committed to providing a friendly, safe and welcoming
  environment for all, regardless of gender, sexual orientation,
  disability, ethnicity, religion, or similar personal characteristic.
* Please avoid using overtly sexual nicknames or other nicknames that
  might detract from a friendly, safe and welcoming environment for
  all.
* Please be kind and courteous. There's no need to be mean or rude.
* Respect that people have differences of opinion and that every
  design or implementation choice carries a trade-off and numerous
  costs. There is seldom a right answer.
* Please keep unstructured critique to a minimum. If you have solid
  ideas you want to experiment with, make a fork and see how it works.
* We will exclude you from interaction if you insult, demean or harass
  anyone.  That is not welcome behaviour. We interpret the term
  "harassment" as including the definition in the [Citizen Code of
  Conduct](http://citizencodeofconduct.org/); if you have any lack of
  clarity about what might be included in that concept, please read
  their definition. In particular, we don't tolerate behavior that
  excludes people in socially marginalized groups.
* Private harassment is also unacceptable. No matter who you are, if
  you feel you have been or are being harassed or made uncomfortable
  by a community member, please contact one of the channel ops or any
  of the TC members immediately with a capture (log, photo, email) of
  the harassment if possible.  Whether you're a regular contributor or
  a newcomer, we care about making this community a safe place for you
  and we've got your back.
* Likewise any spamming, trolling, flaming, baiting or other
  attention-stealing behaviour is not welcome.
* Avoid the use of personal pronouns in code comments or
  documentation. There is no need to address persons when explaining
  code (e.g. "When the developer")
