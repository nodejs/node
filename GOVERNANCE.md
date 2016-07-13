# Node.js Project Governance

## Core Technical Committee

The Node.js project is governed by a Core Technical Committee (CTC) which is
responsible for high-level guidance of the project.

The CTC has final authority over this project including:

* Technical direction
* Project governance and process (including this policy)
* Contribution policy
* GitHub repository hosting
* Conduct guidelines
* Maintaining the list of additional Collaborators

For the current list of CTC members, see the project
[README.md](./README.md#current-project-team-members).

## Collaborators

The [nodejs/node](https://github.com/nodejs/node) GitHub repository is
maintained by the CTC and additional Collaborators who are added by the
CTC on an ongoing basis.

Individuals making significant and valuable contributions are made
Collaborators and given commit-access to the project. These
individuals are identified by the CTC and their addition as
Collaborators is discussed during the weekly CTC meeting.

_Note:_ If you make a significant contribution and are not considered
for commit-access, log an issue or contact a CTC member directly and it
will be brought up in the next CTC meeting.

Modifications of the contents of the nodejs/node repository are made on
a collaborative basis. Anybody with a GitHub account may propose a
modification via pull request and it will be considered by the project
Collaborators. All pull requests must be reviewed and accepted by a
Collaborator with sufficient expertise who is able to take full
responsibility for the change. In the case of pull requests proposed
by an existing Collaborator, an additional Collaborator is required
for sign-off. Consensus should be sought if additional Collaborators
participate and there is disagreement around a particular
modification. See [Consensus Seeking Process](#consensus-seeking-process) below
for further detail on the consensus model used for governance.

Collaborators may opt to elevate significant or controversial
modifications, or modifications that have not found consensus to the
CTC for discussion by assigning the ***ctc-agenda*** tag to a pull
request or issue. The CTC should serve as the final arbiter where
required.

For the current list of Collaborators, see the project
[README.md](./README.md#current-project-team-members).

A guide for Collaborators is maintained in
[COLLABORATOR_GUIDE.md](./COLLABORATOR_GUIDE.md).

### Collaborator Activities

Typical activities of a Collaborator include:

* helping users and novice contributors
* contributing code and documentation changes that improve the project
* reviewing and commenting on issues and pull requests
* participation in working groups
* merging pull requests

While the above are typical things done by Collaborators, there are no required
activities to retain Collaborator status. There is currently no process by which
inactive Collaborators are removed from the project.

## CTC Membership

CTC seats are not time-limited. There is no fixed size of the CTC. The CTC
should be of such a size as to ensure adequate coverage of important areas of
expertise balanced with the ability to make decisions efficiently.

There is no specific set of requirements or qualifications for CTC
membership beyond these rules.

The CTC may add additional members to the CTC by a standard CTC motion.

When a CTC member's participation in [CTC activities](#ctc-activities) has become
minimal for a sustained period of time, the CTC will request that the member
either indicate an intention to increase participation or voluntarily resign.

CTC members may only be removed by voluntary resignation or through a standard
CTC motion.

Changes to CTC membership should be posted in the agenda, and may be
suggested as any other agenda item (see [CTC Meetings](#ctc-meetings) below).

No more than 1/3 of the CTC members may be affiliated with the same
employer.  If removal or resignation of a CTC member, or a change of
employment by a CTC member, creates a situation where more than 1/3 of
the CTC membership shares an employer, then the situation must be
immediately remedied by the resignation or removal of one or more CTC
members affiliated with the over-represented employer(s).

### CTC Activities

Typical activities of a CTC member include:

* attending the weekly meeting
* commenting on the weekly CTC meeting issue and issues labeled `ctc-agenda`
* participating in CTC email threads
* volunteering for tasks that arise from CTC meetings and related discussions
* other activities (beyond those typical of Collaborators) that facilitate the
  smooth day-to-day operation of the Node.js project

Note that CTC members are also Collaborators and therefore typically perform
Collaborator activities as well.

### CTC Meetings

The CTC meets weekly in a voice conference call. The meeting is run by a
designated moderator approved by the CTC. Each meeting is streamed on YouTube.

Items are added to the CTC agenda which are considered contentious or
are modifications of governance, contribution policy, CTC membership,
or release process.

The intention of the agenda is not to approve or review all patches.
That should happen continuously on GitHub and be handled by the larger
group of Collaborators.

Any community member or contributor can ask that something be added to
the next meeting's agenda by logging a GitHub issue. Any Collaborator,
CTC member or the moderator can add the item to the agenda by adding
the ***ctc-agenda*** tag to the issue.

Prior to each CTC meeting, the moderator will share the agenda with
members of the CTC. CTC members can also add items to the agenda at the
beginning of each meeting. The moderator and the CTC cannot veto or remove
items.

The CTC may invite persons or representatives from certain projects to
participate in a non-voting capacity.

The moderator is responsible for summarizing the discussion of each agenda item
and sending it as a pull request after the meeting.

## Consensus Seeking Process

The CTC follows a
[Consensus Seeking](http://en.wikipedia.org/wiki/Consensus-seeking_decision-making)
decision making model.

When an agenda item has appeared to reach a consensus, the moderator will ask
"Does anyone object?" as a final call for dissent from the consensus.

If an agenda item cannot reach a consensus, a CTC member can call for either a
closing vote or a vote to table the issue to the next meeting. All votes
(including votes to close or table) pass if and only if more than 50% of the CTC
members (excluding individuals who explicitly abstain) vote in favor. For
example, if there are 20 CTC members, and 5 of those members indicate that they
abstain, then 8 votes in favor are required for a resolution to pass.
