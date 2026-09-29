---
title: 'Becoming a committer'
description: 'How does one become a V8 committer? This document explains.'
---
Technically, committers are people who have write access to the V8 repository. All patches need to be reviewed by at least two committers (including the author). Independently from this requirement, patches also need to be authored or reviewed by an OWNER.

This privilege is granted with some expectation of responsibility: committers are people who care about the V8 project and want to help meet its goals. Committers are not just people who can make changes, but people who have demonstrated their ability to collaborate with the team, get the most knowledgeable people to review code, contribute high-quality code, and follow through to fix issues (in code or tests).

A committer is a contributor to the V8 project’s success and a citizen helping the projects succeed. See [Committer’s Responsibility](/docs/committer-responsibility).

## How do I become a committer?

*Note to Googlers: There is a [slightly different approach for V8 team members](http://go/v8/setup_permissions.md).*

If you haven't done so already, **you'll need to set up a Security Key on your account before you're added to the committer list.**.  For more information about this requirement see [Gerrit ReAuth](https://chromium.googlesource.com/chromium/src/+/main/docs/gerrit_reauth.md).

In a nutshell, contribute 20 non-trivial patches and get at least three different people to review them (you'll need three people to support you). Then ask someone to nominate you. You're demonstrating your:

- commitment to the project (20 good patches requires a lot of your valuable time),
- ability to collaborate with the team,
- understanding of how the team works (policies, processes for testing and code review, etc),
- understanding of the projects' code base and coding style, and
- ability to write good code (last but certainly not least)

A current committer nominates you by sending email to <v8-committers@chromium.org> containing:

- your first and last name
- your email address in Gerrit
- an explanation of why you should be a committer,
- embedded list of links to revisions (about top 10) containing your patches

Two other committers need to second your nomination. If no one objects in 5 working days, you're a committer.  If anyone objects or wants more information, the committers discuss and usually come to a consensus (within the 5 working days). If issues cannot be resolved, there's a vote among current committers.

Once you get approval from the existing committers, you are granted additional review permissions. You'll also be added to the mailing list v8-committers@chromium.org.

In the worst case, the process can drag out for two weeks. Keep writing patches! Even in the rare cases where a nomination fails, the objection is usually something easy to address like “more patches” or “not enough people are familiar with this person’s work.”

## Maintaining committer status

You don't really need to do much to maintain committer status: just keep being awesome and helping the V8 project!

In the unhappy event that a committer continues to disregard good citizenship (or actively disrupts the project), we may need to revoke that person's status. The process is the same as for nominating a new committer: someone suggests the revocation with a good reason, two people second the motion, and a vote may be called if consensus cannot be reached. I hope that's simple enough, and that we never have to test it in practice.

In addition, as a security measure, if you are inactive on Gerrit (no upload, no comment and no review) for more than a year, we may revoke your committer privileges. An email notification is sent about 7 days prior to the removal. This is not meant as a punishment, so if you wish to resume contributing after that, contact v8-committers@chromium.org to ask that it be restored, and we will normally do so.

(This document was inspired by <https://dev.chromium.org/getting-involved/become-a-committer>.)
