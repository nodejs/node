# RFC: RFC process

The request for comments (RFC) process is intended to provide a consistent and
controlled path for new features to enter Node.js. The process helps find
consensus among Node.js community and collaborators on substantial changes to
the runtime.

## Motivation

Attempting to contribute substantial features to Node.js can result in
interminable, chaotic, and repetitive conversations with no clear resolution and
no realistic process for resolution. The intention of this proposal is to
provide a clearer process.

Examples that may have benefited from an RFC process:

* [Implement window.fetch into core][]

## Guide-level explanation

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

## Reference-level explanation

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

## Drawbacks

* This approach means a formal process for substantial features where we do not
  currently have one. This could be seen as an advantage rather than drawback,
  Nonetheless, all else being equal, it is desirable to have less process. In an
  attempt to address this drawback, this proposal attempts a lightweight
  approach.

## Rationale and alternatives

The current process has resulted in a number of stalemates that have taken a
toll on participants. It works well for bugfixes but not as well for big new
features. Defining a process and reconsidering out decision-making process may
avoid the problem, in the words of one contributor, of "Good ideas die because
people can't agree on a parameter name."

Alternatives include:

* The _status quo_ may not be ideal, but perhaps it works well enough, or better
  than alternatives can be expected to work.
* We could consider a more traditional heavyweight RFC process. There do not
  appear to be any advocates whatsoever for such a thing.
* Instead (or in addition) to an RFC process, we could choose to expand and
  improve the Working Group (WG) model. This (arguably) worked well for ES
  modules. It is, however, harder to imagine it working well for deciding
  whether to implement fetch in Node.js core or whether to bundle package
  managers other than `npm`.

## Prior art

* [Node.js enhancement proposals][]
  * Only a few people wrote enhancement proposals (EPs). A bigger problem may
    have been that only a few people read them. One lesson this proposal tries
    to take from issues with the EP process is that the RFCs need to be visible.
    In general, Node.js information is overly siloed. Rather than placed in a
    separate repository, RFCs will live in the main Node.js core repository.
  * Because the process was, in effect, not mandatory, there were not a lot of
    examples by core team members showing what a good RFC and RFC process look
    like.
* [Rust RFC process][]
  * This is largely what this proposal is modeled on.
* [Python Enhancement Proposals][]
  * This is a more detailed (and thus heavyweight) model than what is proposed
    here.
* [Any interest in an RFC process?][]
  * This TSC issue is what prompted this proposal. It contains more details
    about some background information that is summarized more briefly in this
    proposal.

## Unresolved questions

* This process should be implemented on a trial basis. What is the appropriate
  time period to evaluate how the RFC process is working? What is the process
  for that evaluation? How do we make changes or revoke the process? (TSC
  decision?)
* Should RFCs (and other things perhaps) be consent-based rather than
  consensus-based?
* Should RFC discussions be time-boxed? Getting to "no" faster would be good.
  It's not that the RFC process should lead to success all the time or even
  necessarily most of the time. It's that it should never lead to a years-long
  process which never arrives at a clear resolution one way or the other.
* Should opening a pull request with a minimal implementation could be part of
  the process? This would fail to separate idea from implementation, but it
  would help people who feel they can't determine much with certainty until they
  see how it will be work in practice. (I'm hopeful that the emphasis on
  examples in the template will meet that need.)
* How will this affect velocity? Is that a concern or out of scope?

[Accepted RFCs list]: ./README.md#accepted-rfcs-list
[Any interest in an RFC process?]: https://github.com/nodejs/TSC/issues/962
[Implement window.fetch into core]: https://github.com/nodejs/TSC/issues/962
[Node.js core team collaborators]: ../../README.md#collaborators
[Node.js enhancement proposals]: https://github.com/nodejs/node-eps
[Python Enhancement Proposals]: https://www.python.org/dev/peps/pep-0001/
[Rust RFC process]: https://github.com/rust-lang/rfcs/blob/HEAD/text/0002-rfc-process.md
