# Node.js Project Governance

## Core Technical Committee

The Node.js project is jointly governed by a Core Technical Committee (CTC)
which is responsible for high-level guidance of the project.

The CTC has final authority over this project including:

* Technical direction
* Project governance and process (including this policy)
* Contribution policy
* GitHub repository hosting
* Conduct guidelines
* Maintaining the list of additional Collaborators

Initial membership invitations to the CTC were given to individuals who
had been active contributors to Node.js, and who have significant
experience with the management of the Node.js project. Membership is
expected to evolve over time according to the needs of the project.

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
modification. See _Consensus Seeking Process_ below for further detail
on the consensus model used for governance.

Collaborators may opt to elevate significant or controversial
modifications, or modifications that have not found consensus to the
CTC for discussion by assigning the ***ctc-agenda*** tag to a pull
request or issue. The CTC should serve as the final arbiter where
required.

For the current list of Collaborators, see the project
[README.md](./README.md#current-project-team-members).

A guide for Collaborators is maintained in
[COLLABORATOR_GUIDE.md](./COLLABORATOR_GUIDE.md).

## CTC Membership

CTC seats are not time-limited. There is no fixed size of the CTC. The CTC
should be of such a size as to ensure adequate coverage of important areas of
expertise balanced with the ability to make decisions efficiently.

There is no specific set of requirements or qualifications for CTC
membership beyond these rules.

The CTC may add additional members to the CTC by a standard CTC motion.

A CTC member may be removed from the CTC by voluntary resignation, or by
a standard CTC motion.

Changes to CTC membership should be posted in the agenda, and may be
suggested as any other agenda item (see "CTC Meetings" below).

No more than 1/3 of the CTC members may be affiliated with the same
employer.  If removal or resignation of a CTC member, or a change of
employment by a CTC member, creates a situation where more than 1/3 of
the CTC membership shares an employer, then the situation must be
immediately remedied by the resignation or removal of one or more CTC
members affiliated with the over-represented employer(s).

## CTC Meetings

The CTC meets weekly on a Google Hangout On Air. The meeting is run by
a designated moderator approved by the CTC. Each meeting should be
published to YouTube.

Items are added to the CTC agenda which are considered contentious or
are modifications of governance, contribution policy, CTC membership,
or release process.

The intention of the agenda is not to approve or review all patches.
That should happen continuously on GitHub and be handled by the larger
group of Collaborators.

Any community member or contributor can ask that something be added to
the next meeting's agenda by logging a GitHub Issue. Any Collaborator,
CTC member or the moderator can add the item to the agenda by adding
the ***ctc-agenda*** tag to the issue.

Prior to each CTC meeting, the moderator will share the Agenda with
members of the CTC. CTC members can add any items they like to the
agenda at the beginning of each meeting. The moderator and the CTC
cannot veto or remove items.

The CTC may invite persons or representatives from certain projects to
participate in a non-voting capacity.

The moderator is responsible for summarizing the discussion of each
agenda item and sending it as a pull request after the meeting.

## Consensus Seeking Process

The CTC follows a
[Consensus Seeking](http://en.wikipedia.org/wiki/Consensus-seeking_decision-making)
decision making model.

When an agenda item has appeared to reach a consensus, the moderator
will ask "Does anyone object?" as a final call for dissent from the
consensus.

If an agenda item cannot reach a consensus, a CTC member can call for
either a closing vote or a vote to table the issue to the next
meeting. The call for a vote must be approved by a majority of the CTC
or else the discussion will continue. Simple majority wins.
