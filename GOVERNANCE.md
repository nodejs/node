# Node.js Project Governance

## Technical Committee

The Node.js project is jointly governed by a Technical Steering Committee (TSC)
which is responsible for high-level guidance of the project.

The TSC has final authority over this project including:

* Technical direction
* Project governance and process (including this policy)
* Contribution policy
* GitHub repository hosting
* Conduct guidelines
* Maintaining the list of additional Collaborators

Initial membership invitations to the TSC were given to individuals who
had been active contributors to Node.js, and who have significant
experience with the management of the Node.js project. Membership is
expected to evolve over time according to the needs of the project.

For the current list of TSC members, see the project
[README.md](./README.md#current-project-team-members).

## Collaborators

The [nodejs/node](https://github.com/nodejs/node) GitHub repository is
maintained by the TC and additional Collaborators who are added by the
TC on an ongoing basis.

Individuals making significant and valuable contributions are made
Collaborators and given commit-access to the project. These
individuals are identified by the TC and their addition as
Collaborators is discussed during the weekly TC meeting.

_Note:_ If you make a significant contribution and are not considered
for commit-access, log an issue or contact a TC member directly and it
will be brought up in the next TC meeting.

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
TC for discussion by assigning the ***tc-agenda*** tag to a pull
request or issue. The TC should serve as the final arbiter where
required.

For the current list of Collaborators, see the project
[README.md](./README.md#current-project-team-members).

A guide for Collaborators is maintained in
[COLLABORATOR_GUIDE.md](./COLLABORATOR_GUIDE.md).

## TC Membership

TC seats are not time-limited.  There is no fixed size of the TC.
However, the expected target is between 6 and 12, to ensure adequate
coverage of important areas of expertise, balanced with the ability to
make decisions efficiently.

There is no specific set of requirements or qualifications for TC
membership beyond these rules.

The TC may add additional members to the TC by a standard TC motion.

A TC member may be removed from the TC by voluntary resignation, or by
a standard TC motion.

Changes to TC membership should be posted in the agenda, and may be
suggested as any other agenda item (see "TC Meetings" below).

No more than 1/3 of the TC members may be affiliated with the same
employer.  If removal or resignation of a TC member, or a change of
employment by a TC member, creates a situation where more than 1/3 of
the TC membership shares an employer, then the situation must be
immediately remedied by the resignation or removal of one or more TC
members affiliated with the over-represented employer(s).

## TC Meetings

The TC meets weekly on a Google Hangout On Air. The meeting is run by
a designated moderator approved by the TC. Each meeting should be
published to YouTube.

Items are added to the TC agenda which are considered contentious or
are modifications of governance, contribution policy, TC membership,
or release process.

The intention of the agenda is not to approve or review all patches.
That should happen continuously on GitHub and be handled by the larger
group of Collaborators.

Any community member or contributor can ask that something be added to
the next meeting's agenda by logging a GitHub Issue. Any Collaborator,
TC member or the moderator can add the item to the agenda by adding
the ***tc-agenda*** tag to the issue.

Prior to each TC meeting, the moderator will share the Agenda with
members of the TC. TC members can add any items they like to the
agenda at the beginning of each meeting. The moderator and the TC
cannot veto or remove items.

The TC may invite persons or representatives from certain projects to
participate in a non-voting capacity. These invitees currently are:

* A representative from [build](https://github.com/node-forward/build)
  chosen by that project.

The moderator is responsible for summarizing the discussion of each
agenda item and sending it as a pull request after the meeting.

## Consensus Seeking Process

The TC follows a
[Consensus Seeking](http://en.wikipedia.org/wiki/Consensus-seeking_decision-making)
decision making model.

When an agenda item has appeared to reach a consensus, the moderator
will ask "Does anyone object?" as a final call for dissent from the
consensus.

If an agenda item cannot reach a consensus, a TC member can call for
either a closing vote or a vote to table the issue to the next
meeting. The call for a vote must be approved by a majority of the TC
or else the discussion will continue. Simple majority wins.
