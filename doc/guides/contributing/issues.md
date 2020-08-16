# Issues

* [How to Contribute in Issues](#how-to-contribute-in-issues)
* [Asking for General Help](#asking-for-general-help)
* [Discussing non-technical topics](#discussing-non-technical-topics)
* [Submitting a Bug Report](#submitting-a-bug-report)
* [Triaging a Bug Report](#triaging-a-bug-report)
* [Resolving a Bug Report](#resolving-a-bug-report)

## How to Contribute in Issues

For any issue, there are fundamentally three ways an individual can
contribute:

1. By opening the issue for discussion: For instance, if you believe that you
   have uncovered a bug in Node.js, creating a new issue in the `nodejs/node`
   issue tracker is the way to report it.
2. By helping to triage the issue: This can be done either by providing
   supporting details (a test case that demonstrates a bug), or providing
   suggestions on how to address the issue.
3. By helping to resolve the issue: Typically this is done either in the form
   of demonstrating that the issue reported is not a problem after all, or more
   often, by opening a Pull Request that changes some bit of something in
   `nodejs/node` in a concrete and reviewable manner.

## Asking for General Help

Because the level of activity in the `nodejs/node` repository is so high,
questions or requests for general help using Node.js should be directed at
the [Node.js help repository][].

## Discussing non-technical topics

Discussion of non-technical topics (such as intellectual property and trademark)
should be directed to the [Technical Steering Committee (TSC) repository][].

## Submitting a Bug Report

When opening a new issue in the `nodejs/node` issue tracker, users will be
presented with a basic template that should be filled in.

```markdown
<!--
Thank you for reporting an issue.

This issue tracker is for bugs and issues found within Node.js core.
If you require more general support please file an issue on our help
repo. https://github.com/nodejs/help


Please fill in as much of the template below as you're able.

Version: output of `node -v`
Platform: output of `uname -a` (UNIX), or version and 32 or 64-bit (Windows)
Subsystem: if known, please specify affected core module name

If possible, please provide code that demonstrates the problem, keeping it as
simple and free of external dependencies as you are able.
-->

* **Version**:
* **Platform**:
* **Subsystem**:

<!-- Enter your issue details below this comment. -->
```

If you believe that you have uncovered a bug in Node.js, please fill out this
form, following the template to the best of your ability. Do not worry if you
cannot answer every detail, just fill in what you can.

The two most important pieces of information we need in order to properly
evaluate the report is a description of the behavior you are seeing and a simple
test case we can use to recreate the problem on our own. If we cannot recreate
the issue, it becomes impossible for us to fix.

In order to rule out the possibility of bugs introduced by userland code, test
cases should be limited, as much as possible, to using *only* Node.js APIs.
If the bug occurs only when you're using a specific userland module, there is
a very good chance that either (a) the module has a bug or (b) something in
Node.js changed that broke the module.

See [How to create a Minimal, Complete, and Verifiable example](https://stackoverflow.com/help/mcve).

## Triaging a Bug Report

Once an issue has been opened, it is common for there to be discussion
around it. Some contributors may have differing opinions about the issue,
including whether the behavior being seen is a bug or a feature. This discussion
is part of the process and should be kept focused, helpful, and professional.

The objective of helping with triaging issues (in core and help repos) is to
help reduce the issue backlog and keep the issue tracker healthy, while enabling
newcomers another meaningful way to get engaged and contribute.

Anyone with a reasonable understanding of Node.js programming and the
project's GitHub organization plus a few contributions to the project
(commenting on issues or PRs) can apply for and become a triager. Open a PR
on the README.md of this project with: i) a request to be added as a triager,
ii) the motivation for becoming a triager, and iii) agreement on reading,
understanding, and adhering to the project's [Code Of Conduct](https://github.com/nodejs/admin/blob/master/CODE_OF_CONDUCT.md).

The triage role enables the ability to carry out the most common triage
activities, such as applying labels and closing/reopening/assigning issues.
For more information on the roles and permissions, see ["Permission levels for
repositories owned by an organization"](https://docs.github.com/en/github/setting-up-and-managing-organizations-and-teams/repository-permission-levels-for-an-organization#permission-levels-for-repositories-owned-by-an-organization).

## Resolving a Bug Report

In the vast majority of cases, issues are resolved by opening a Pull Request.
The process for opening and reviewing a Pull Request is similar to that of
opening and triaging issues, but carries with it a necessary review and approval
workflow that ensures that the proposed changes meet the minimal quality and
functional guidelines of the Node.js project.

[Node.js help repository]: https://github.com/nodejs/help/issues
[Technical Steering Committee (TSC) repository]: https://github.com/nodejs/TSC/issues
