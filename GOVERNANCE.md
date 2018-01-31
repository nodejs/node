# Node.js Project Governance

The Node.js project is governed by its Collaborators, including a Technical
Steering Committee (TSC) which is responsible for high-level guidance of the
project.

<!-- TOC -->

- [Collaborators](#collaborators)
  - [Collaborator Activities](#collaborator-activities)
- [Technical Steering Committee](#technical-steering-committee)
  - [TSC Meetings](#tsc-meetings)
- [Collaborator Nominations](#collaborator-nominations)
  - [Onboarding](#onboarding)
- [Consensus Seeking Process](#consensus-seeking-process)

<!-- /TOC -->

## Collaborators

The [nodejs/node][] GitHub repository is maintained by Node.js Core
Collaborators. Upon becoming Collaborators, they:

* Become members of the @nodejs/collaborators team
* Gain individual membership of the Node.js foundation

Their privileges include but are not limited to:

* Commit access to the [nodejs/node][] repository
* Access to the Node.js continuous integration (CI) jobs

Modifications of the contents of the nodejs/node repository are made on
a collaborative basis. Anybody with a GitHub account may propose a
modification via pull request and it will be considered by the project
Collaborators. All pull requests must be reviewed and accepted by a
Collaborator with sufficient expertise who is able to take full
responsibility for the change. In the case of pull requests proposed
by an existing Collaborator, an additional Collaborator is required
for sign-off.

If one or more Collaborators oppose a proposed change, then the change can not
be accepted unless:

* Discussions and/or additional changes result in no Collaborators objecting to
  the change. Previously-objecting Collaborators do not necessarily have to
  sign-off on the change, but they should not be opposed to it.
* The change is escalated to the TSC and the TSC votes to approve the change.
  This should only happen if disagreements between Collaborators cannot be
  resolved through discussion.

Collaborators may opt to elevate significant or controversial modifications to
the TSC by assigning the `tsc-review` label to a pull request or issue. The
TSC should serve as the final arbiter where required.

See:

* [Current list of Collaborators](./README.md#current-project-team-members)
* [A guide for Collaborators](./COLLABORATOR_GUIDE.md)

### Collaborator Activities

Typical activities of a Collaborator include:

* Helping users and novice contributors
* Contributing code and documentation changes that improve the project
* Reviewing and commenting on issues and pull requests
* Participation in working groups
* Merging pull requests

The TSC periodically reviews the Collaborator list to identify inactive
Collaborators. Past Collaborators are typically given _Emeritus_ status. Emeriti
may request that the TSC restore them to active status.

## Technical Steering Committee

A subset of the Collaborators form the Technical Steering Committee (TSC).
The TSC has final authority over this project, including:

* Technical direction
* Project governance and process (including this policy)
* Contribution policy
* GitHub repository hosting
* Conduct guidelines
* Maintaining the list of additional Collaborators

The current list of TSC members can be found in
[the project README](./README.md#current-project-team-members).

The operations of the TSC are governed by the [TSC Charter][] as approved by
the Node.js Foundation Board of Directors.

### TSC Meetings

The TSC meets regularly in a voice conference call. The meeting is run by a
designated meeting chair approved by the TSC. Each meeting is streamed on
YouTube.

Items are added to the TSC agenda which are considered contentious or
are modifications of governance, contribution policy, TSC membership,
or release process.

The intention of the agenda is not to approve or review all patches.
That should happen continuously on GitHub and be handled by the larger
group of Collaborators.

Any community member or contributor can ask that something be reviewed
by the TSC by logging a GitHub issue. Any Collaborator, TSC member, or the
meeting chair can bring the issue to the TSC's attention by applying the
`tsc-review` label. If consensus-seeking among TSC members fails for a
particular issue, it may be added to the TSC meeting agenda by adding the
`tsc-agenda` label.

Prior to each TSC meeting, the meeting chair will share the agenda with
members of the TSC. TSC members can also add items to the agenda at the
beginning of each meeting. The meeting chair and the TSC cannot veto or remove
items.

The TSC may invite additional persons to participate in a non-voting capacity.

The meeting chair is responsible for ensuring that minutes are taken and that a
pull request with the minutes is submitted after the meeting.

Due to the challenges of scheduling a global meeting with participants in
several timezones, the TSC will seek to resolve as many agenda items as possible
outside of meetings using
[the TSC issue tracker](https://github.com/nodejs/TSC/issues). The process in
the issue tracker is:

* A TSC member opens an issue explaining the proposal/issue and @-mentions
  @nodejs/tsc.
* After 72 hours, if there are two or more `LGTM`s from other TSC members and no
  explicit opposition from other TSC members, then the proposal is approved.
* If there are any TSC members objecting, then a conversation ensues until
  either the proposal is dropped or the objecting members are persuaded. If
  there is an extended impasse, a motion for a vote may be made.

## Collaborator Nominations

Any existing Collaborator can nominate an individual making significant
and valuable contributions across the Node.js organization to become a new
Collaborator.

To nominate a new Collaborator, open an issue in the [nodejs/node][]
repository, with a summary of the nominee's contributions, for example:

* Commits in the [nodejs/node][] repository.
  * Can be shown using the link
    `https://github.com/nodejs/node/commits?author=${GITHUB_ID}`
    (replace `${GITHUB_ID}` with the nominee's GitHub ID).
* Pull requests and issues opened in the [nodejs/node][] repository.
  * Can be shown using the link
    `https://github.com/nodejs/node/pulls?q=author%3A${GITHUB_ID}+`
* Comments and reviews on issues and pull requests in the
  [nodejs/node][] repository
  * Can be shown using the links
    `https://github.com/nodejs/node/pulls?q=reviewed-by%3A${GITHUB_ID}+`
    and `https://github.com/nodejs/node/pulls?q=commenter%3A${GITHUB_ID}+`
* Assistance provided to end users and novice contributors
* Participation in other projects, teams, and working groups of the
  Node.js organization
  * Can be shown using the links
    `https://github.com/search?q=author%3A${GITHUB_ID}++org%3Anodejs&type=Issues`
    and
    `https://github.com/search?q=commenter%3A${GITHUB_ID}++org%3Anodejs&type=Issues`
* Other participation in the wider Node.js community

Mention @nodejs/collaborators in the issue to notify other Collaborators about
the nomination.

If there are no objections raised by any Collaborators one week after
the issue is opened, the nomination will be considered as accepted.
Should there be any objections against the nomination, the TSC is responsible
for working with the individuals involved and finding a resolution.
The nomination must be approved by the TSC, which is assumed when there are no
objections from any TSC members.

Prior to the public nomination, the Collaborator initiating it can seek
feedback from other Collaborators in private using
[the GitHub discussion page][collaborators-discussions] of the
Collaborators team, and work with the nominee to improve the nominee's
contribution profile, in order to make the nomination as frictionless
as possible.

If individuals making valuable contributions do not believe they have been
considered for a nomination, they may log an issue or contact a Collaborator
directly.

### Onboarding

When the nomination is accepted, the new Collaborator will be onboarded
by a TSC member. See [the onboarding guide](./doc/onboarding.md) on
details of the onboarding process. In general, the onboarding should be
completed within a month after the nomination is accepted.

## Consensus Seeking Process

The TSC follows a [Consensus Seeking][] decision making model as described by
the [TSC Charter][].

[collaborators-discussions]: https://github.com/orgs/nodejs/teams/collaborators/discussions
[Consensus Seeking]: https://en.wikipedia.org/wiki/Consensus-seeking_decision-making
[TSC Charter]: https://github.com/nodejs/TSC/blob/master/TSC-Charter.md
[nodejs/node]: https://github.com/nodejs/node
