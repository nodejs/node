# Node.js Project Governance

The Node.js project is governed by its Collaborators, including a Technical
Steering Committee (TSC) which is responsible for high-level guidance of the
project.

## Collaborators

The [nodejs/node](https://github.com/nodejs/node) GitHub repository is
maintained by Collaborators who are added by the TSC on an ongoing basis.

Individuals identified by the TSC as making significant and valuable
contributions across any Node.js repository may be made Collaborators and given
commit access to the project. Activities taken into consideration include (but
are not limited to) the quality of:

* code commits and pull requests
* documentation commits and pull requests
* comments on issues and pull requests
* contributions to the Node.js website
* assistance provided to end users and novice contributors
* participation in Working Groups
* other participation in the wider Node.js community

If individuals making valuable contributions do not believe they have been
considered for commit access, they may log an issue or contact a TSC member
directly.

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

* [Current list of Collaborators](./README.md#current-project-team-members)
* [A guide for Collaborators](./COLLABORATOR_GUIDE.md)

### Collaborator Activities

Typical activities of a Collaborator include:

* helping users and novice contributors
* contributing code and documentation changes that improve the project
* reviewing and commenting on issues and pull requests
* participation in working groups
* merging pull requests

The TSC periodically reviews the Collaborator list to identify inactive
Collaborators. Past Collaborators are typically given _Emeritus_ status. Emeriti
may request that the TSC restore them to active status.

## Technical Steering Committee

The Technical Steering Committee (TSC) has final authority over this project
including:

* Technical direction
* Project governance and process (including this policy)
* Contribution policy
* GitHub repository hosting
* Conduct guidelines
* Maintaining the list of additional Collaborators

* [Current list of TSC members](./README.md#current-project-team-members)

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

## Consensus Seeking Process

The TSC follows a [Consensus Seeking][] decision making model as described by
the [TSC Charter][].

[TSC Charter]: https://github.com/nodejs/TSC/blob/master/TSC-Charter.md
[Consensus Seeking]: http://en.wikipedia.org/wiki/Consensus-seeking_decision-making
