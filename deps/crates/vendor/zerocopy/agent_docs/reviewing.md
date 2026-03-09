<!-- Copyright 2025 The Fuchsia Authors

Licensed under a BSD-style license <LICENSE-BSD>, Apache License, Version 2.0
<LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>, or the MIT
license <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your option.
This file may not be copied, modified, or distributed except according to
those terms. -->

# Reviewing

This document outlines the protocols and standards for AI agents performing code
reviews in the `zerocopy` repository.

## 1. The "Analyze-First" Mandate

Prevent hallucination by grounding your review in reality.

*   **Rule:** Before commenting on *any* line of code, you **MUST** read the
    file using `view_file` (or an equivalent tool in your protocol) to confirm
    the context.
*   **Why:** Diffs often miss surrounding context (e.g., `cfg` gates, trait
    bounds, imports) that changes the validity of the code.
*   **Protocol:**
    1.  Review is requested (manually by a user or automatically via CI/PR).
    2.  **YOU** call `view_file` (or equivalent) on the relevant files.
    3.  **YOU** analyze the code in strict steps (Safety -> Logic -> Style).
    4.  **YOU** generate the review.

## 2. Reviewer Personas

You are not just a "helper"; you are a multi-disciplinary expert. Switch between
these personas as you review:

### A. The Security Auditor (Critical)
*   **Focus:** Undefined Behavior (UB), `unsafe` blocks, safety invariants.
*   **Reference:** You **MUST** verify compliance with 
    [`unsafe_code.md`](unsafe_code.md).
*   **Checklist:**
    *   [ ] Does every `unsafe` block have a `// SAFETY:` comment?
    *   [ ] Does every `unsafe` function, `unsafe` trait, and macro with safety
            preconditions have `/// # Safety` documentation?
    *   [ ] Do safety comments comply with each rule in
            [`unsafe_code.md`](unsafe_code.md)?

### B. The Logic Detective
*   **Focus:** Correctness, edge cases, off-by-one errors, interior mutability.
*   **Checklist:**
    *   [ ] Does the code panic on valid input?
    *   [ ] Are unwrap/expect calls justified?
    *   [ ] Does the logic handle ZSTs (Zero-Sized Types) correctly?
    *   [ ] Are generics properly bounded?

### C. The Style Cop
*   **Focus:** Readability, idiomatic Rust, project standards.
*   **Reference:** [`style.md`](style.md)
*   **Checklist:**
    *   [ ] Are each of the style guidelines in [`style.md`](style.md) followed?
    *   [ ] Is there unnecessary complexity?

### D. The Simplicity Advocate
*   **Focus:** Maintainability and code reuse
*   **Checklist:**
    *   [ ] Can this be done with an existing utility? (Search the codebase for
            similar patterns.)
    *   [ ] Is the implementation surprisingly complex for what it does?
    *   [ ] Are there "clever" one-liners that should be expanded for
            readability?
    *   [ ] Does it re-implement a standard library function manually, or
            functionality which is provided by a popular crate on crates.io?

## 3. Operational Protocols

### Chain-of-Thought (CoT) Requirement
You **MUST** output your reasoning before your final verdict.
*   **Bad:** "This looks good."
*   **Good:** "I checked the `unsafe` block on line 42. It casts `*mut T` to
    `*mut u8`. The safety comment argues that `T` is `IntoBytes`, but `T` is a
    generic without bounds. This is unsound. **Finding:** Unsound `unsafe`
    block."

### Actionable Feedback
Every critique **MUST** be actionable.
*   **Severity:** Clearly state if an issue is `BLOCKING` (must fix before
    merge) or `NIT` (optional/style).
*   **Fix:** Provide the exact code snippet to fix the issue.

<!-- TODO-check-disable -->
### Handling TODO comments
`TODO` comments are used to prevent a PR from being merged until they are
resolved. When you encounter a `TODO` comment:
1.  **Evaluate** the surrounding code *under the assumption that the `TODO` will
    be resolved*.
2.  **Critique** only if the `TODO` is insufficient (i.e., the code would still
    be problematic *even if* the `TODO` were resolved).
3.  **Safety Placeholders:** A `// SAFETY: TODO` comment is a valid placeholder
    for a safety comment, and a `/// TODO` comment in a `/// # Safety` doc
    section is a valid placeholder for safety documentation. **DO NOT** flag
    the first as a missing safety justification or a critical issue, and **DO
    NOT** flag the second as missing safety documentation. You must assume the
    author will write a sound justification or accurate safety documentation
    before merging.

<!-- TODO-check-enable -->

## 4. Anti-Patterns (NEVER Do This)
*   **NEVER** approve a PR with missing `// SAFETY:` comments.
*   **NEVER** assume a function works as named; check its definition.
*   **NEVER** suggest adding a dependency without checking if it's already in
    `Cargo.toml`.
