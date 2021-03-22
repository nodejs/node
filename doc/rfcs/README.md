# Node.js Core RFCs

The request for comments (RFC) process is intended to provide a consistent and
controlled path for new features to enter Node.js. The process helps find
consensus among Node.js community and collaborators on substantial changes to
the runtime.

## Accepted RFCs list

There are no accepted RFCs yet.

## When you need to follow this process

You need to follow this process if you intend to make substantial changes to the
Node.js runtime. Substantial changes include:

* Semantic or syntactic changes to Node.js that are not bugfixes.
* Removing stable Node.js features.
* New modules.
* Bundling of new executables or removal of existing bundled executables.

Changes that do not require an RFC are changes that do not meet the preceding
criteria. These might include:

* Rephrasing text or reorganizing documentation.
* Refactoring code.
* Bugfixes.
* Changes that improve performance benchmarks.
* Additions that only affect core developers and not end users.

If you submit a pull request to implement a new feature without going through
the RFC process, it might be closed with a polite request to submit an RFC
first.

## What the process is

To get a substantial feature added to Node.js, first get the RFC merged into the
`rfcs` directory as a markdown file. At that point the RFC is _accepted_ and may
be implemented with the goal of eventual inclusion into Node.js.

* Fork the RFC repo <https://github.com/nodejs/node>
* Copy `doc/rfcs/00000-template.md` to `doc/rfcs/00000-my-feature.md` where
  `my-feature` is descriptive. Don't assign an RFC number yet.
* Fill in the RFC.
* Submit a pull request. The pull request is where we get review of the
design from the larger community.
* Once the pull request is opened, the submitter or a core team collaborator
  should:
  * Update the RFC file name to change `00000` to the pull request identifier.
  * Update `doc/rfcs/README.md` in the pull request to include the RFC in the
    [Accepted RFCs list][].
* Build consensus and integrate feedback. RFCs that have broad support are much
more likely to make progress than those that don't receive any comments.

Eventually, one of the [Node.js core team collaborators][] will either accept
the RFC by merging the pull request, at which point the RFC is accepted, or
reject it by closing the pull request.

Once an RFC becomes active then authors may implement it and submit the
feature as a pull request to the Node.js repository. _Active_ is not a rubber
stamp, and in particular still does not mean the feature will ultimately be
merged. It does mean that in principle all the major stakeholders have agreed to
the feature and are amenable to merging it.

Modifications to active RFCs can be done in subsequent PRs. An RFC that makes it
through the entire process to implementation is _complete_ and is removed from
the [Accepted RFCs List][].

## What the process tries to achieve

* Discourage unactionable or vague feature requests.
* Ensure that all serious RFCs are considered equally.
* Give confidence to those with a stake in Node.js development that they
  understand why new features are being merged.

## Acknowledgments

The initial proposal for this process is inspired by, modeled on, and partially
copied from the [Rust RFC process][]

[Accepted RFCs list]: #accepted-rfcs-list
[Node.js core team collaborators]: ../../README.md#collaborators
[Rust RFC process]: https://github.com/rust-lang/rfcs/blob/HEAD/text/0002-rfc-process.md
