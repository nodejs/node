# AI use policy and guidelines

* [Core principle](#core-principle)
* [Using AI for code contributions](#using-ai-for-code-contributions)
* [Using AI for communication](#using-ai-for-communication)

## Core principle

Node.js expects contributors to understand and take full responsibility for
every change they propose. The answer to "Why is X an improvement?" should
never be "I'm not sure. The AI did it."

Pull requests that consist of AI-generated code the contributor has not
personally understood, tested, and verified waste collaborator time and
will be subject to closure without additional review.

## Using AI for code contributions

AI tools may assist contributors, but must not replace contributor judgment.
When using AI as a coding assistant:

* **Understand the codebase first.** Do not skip familiarizing yourself with
  the relevant subsystem. LLMs frequently produce inaccurate descriptions of
  Node.js internals — always verify against the actual source. When using an AI
  tool, ask it to cite the exact source files/PRs/docs it’s relying on, and then
  match the claim against that resource to verify if it holds up in the current
  code.

* **Own every line you submit.** You are responsible for all code in your
  pull request, regardless of how it was generated. Be prepared to explain
  any change in detail during review.

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

* **Edit generated comments critically.** LLM-produced comments are often
  verbose or inaccurate. Remove comments that simply restate what the code
  does; add comments only where the logic is non-obvious.

## Using AI for communication

Node.js values concise, precise communication that respects collaborator time.

* **Do not post messages generated entirely by AI** in pull requests, issues, or the
  project's communication channels.
* **Verify accuracy** of any LLM-generated content before including it in a
  PR description or comment.
* **Complete pull request templates fully** rather than replacing them with
  LLM-generated summaries.
* **Link to primary sources** — code, documentation, specifications — rather
  than quoting LLM answers.
* Grammar and spell-check tools are acceptable when they improve clarity and
  conciseness.

[commit message guidelines]: ./pull-requests.md#commit-message-guidelines
