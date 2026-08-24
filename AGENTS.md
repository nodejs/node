# Agents and Automated Tools

This document outlines rules and requirements for AI automation agents and automated
tooling that interact with the Node.js project.

Existing Node.js collaborators (as listed in the README.md) may use AI agents to
contribute but must do so responsibly.

New Node.js contributors should avoid using AI agents to interact with the project.

## Code Contributions

* **No unreviewed automation**: Automated pull requests must not be created without
  ongoing human oversight. The pull request must be actively maintained by a human
  contributor who responds to feedback. The AI agent is not permitted to create pull
  requests, open issues, post comments, respond to reviews, or push commits without
  ongoing human oversight.

* **Commit responsibility**: Commits created with agent assistance must follow all
  Node.js [commit message guidelines](./doc/contributing/pull-requests.md#commit-message-guidelines).
  The human contributor opening the PR takes full responsibility for the changes.

* **Testing and verification**: All changes must pass the Node.js continuous integration.
  Human judgment must verify that existing tests are not removed or modified inappropriately,
  and that new tests correctly validate the intended behavior.

### Requirements

* All commits must be signed off by the human user using `Signed-off-by: <your name> (<your email>)`
  as an attestation to the [Developer Certificate of Origin](https://developercertificate.org/).
* AI-assistance must be acknowledged using the `Assisted-by: <agent name>` annotation.
* AI-authored code contributions must be compatible with the project's licensing and
  contribution guidelines.

## Prohibited Activities

AI agents **must not**:

* Push to any branch or tag in nodejs/node.
* Create unsupervised pull requests or issues without active human engagement.
* Make claims about code without human verification against actual source code.
* Remove or modify existing tests without human judgment.
* Interact with the repository through means other than those explicitly authorized.
* Use commit messages to promote for-profit AI tools or commercial brands. A single
  `Assisted-by: <agent-name>` annotation is required disclosure, not promotion, and is
  permitted.
* Post AI-generated messages directly into pull requests, issues, or project communication
  channels without direct human review and editing to ensure clarity, accuracy, and respect
  for collaborator time.
* Sign off commits using `Signed-off-by: <agent name>` or `Co-authored-by: <agent name>`.

## Violations

Automated interactions that violate these rules may result in:

* Immediate closure of pull requests without review.
* Blocking of the automation tool from further interaction with the project.
* Blocking of the tool's account or its owner from contributing.
* Reports to relevant platforms or organizations operating the automation.

***

For more information on AI use in general contributions (not specific to agents),
see [AI use policy and guidelines](./doc/contributing/ai-guidelines.md).
