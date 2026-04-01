# AI use policy and guidelines

* [Core principle](#core-principle)
* [Using AI for code contributions](#using-ai-for-code-contributions)
* [Using AI for communication](#using-ai-for-communication)

This document aligns with the [OpenJS Foundation AI Coding Assistants Policy][].

## Core principle

Node.js expects contributions to come from _people_. Contributors are free
to use whatever tools they choose, including AI assistants, but such tools
never replace the contributor's own understanding and responsibility.

Node.js requires contributors to understand and take full responsibility for
every change they propose. The answer to "Why is X an improvement?" can
never be "I'm not sure. The AI did it."

If AI tools assisted in generating a contribution, that should be
acknowledged honestly (e.g., via an `Assisted-by:` tag in the commit
metadata) so that reviewers have appropriate context.

Pull requests that consist of AI-generated code the contributor has not
personally understood, tested, and verified waste collaborator time and
will be subject to closure without additional review. Contributors who
repeatedly submit such changes, show no understanding of the project or
its processes, or are dishonest about the use of automated assistance
may be blocked from further contributions.

Pull requests must not be opened by automated tooling not specifically
approved in advance by the project.

## Using AI for code contributions

Contributors may use AI tools to assist with contributions, but such tools
never replace human judgment.

When using AI as a coding assistant:

* **Understand the codebase first.** Do not skip familiarizing yourself with
  the relevant subsystem. LLMs frequently produce inaccurate descriptions of
  Node.js internals — always verify against the actual source. When using an AI
  tool, ask it to cite the exact source it’s relying on, and then
  match the claim against that resource to verify if it holds up in the current
  code.

* **Own every line you submit.** You are responsible for all code in your
  pull request, regardless of how it was generated. This includes ensuring
  that AI-generated or AI-assisted contributions satisfy the project's
  [Developer's Certificate of Origin][] and licensing requirements. Be
  prepared to explain any change in detail during review.

* **Keep logical commits.** Structure commits coherently even when an LLM
  generates multiple changes at once. Follow the existing
  [commit message guidelines][].

* **Test thoroughly.** AI-generated code must pass the full test suite and
  any manually written tests relevant to the change. Existing tests should not
  be removed or modified without human verification. Do not rely on the LLM
  to assess correctness.

* **Do not disappear.** If you open a PR, follow it through. Respond to
  feedback and iterate until the work lands or is explicitly closed. If you
  can no longer pursue it, close the PR. Stalled PRs block progress.

* **Do not use AI to claim "good first issue" tasks.** These issues exist to
  help new contributors learn the codebase and processes hands-on.

* **Edit generated comments critically.** LLM-produced comments are often
  verbose or inaccurate. Remove comments that simply restate what the code
  does; add comments only where the logic is non-obvious.

## Using AI for communication

Node.js values concise, precise communication that respects collaborator and contributor time.

* **Do not post messages generated entirely by AI** in pull requests, issues, or the
  project's communication channels.
* **Verify accuracy** of any LLM-generated content before including it in a
  PR description or comment.
* **Link to primary sources** — code, documentation, specifications — rather
  than quoting LLM answers or linking to LLM chats.
* Grammar and spell-check tools are acceptable when they improve clarity and
  conciseness.

[Developer's Certificate of Origin]: ../../CONTRIBUTING.md#developers-certificate-of-origin-11
[commit message guidelines]: ./pull-requests.md#commit-message-guidelines
[OpenJS Foundation AI Coding Assistants Policy]: https://openjsf.cdn.prismic.io/openjsf/aca4d5GXnQHGZDiZ_OpenJS_AI_Coding_Assistants_Policy.pdf
