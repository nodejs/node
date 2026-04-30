---
title: 'Respectful code'
description: 'Inclusivity is central to V8’s culture, and our values include treating each other with dignity. As such, it’s important that everyone can contribute without facing the harmful effects of bias and discrimination.'
---

Inclusivity is central to V8’s culture, and our values include treating each other with dignity. As such, it’s important that everyone can contribute without facing the harmful effects of bias and discrimination. However, terms in our codebase, UIs, and documentation can perpetuate that discrimination. This document sets forth guidance which aims to address disrespectful terminology in code and documentation.

## Policy { #policy }

Terminology that is derogatory, hurtful, or perpetuates discrimination, either directly or indirectly, should be avoided.

## What is in scope for this policy? { #scope }

Anything that a contributor would read while working on V8, including:

- Names of variables, types, functions, files, build rules, binaries, exported variables, ...
- Test data
- System output and displays
- Documentation (both inside and outside of source files)
- Commit messages

## Principles { #principles }

- Be respectful: derogatory language shouldn’t be necessary to describe how things work.
- Respect culturally sensitive language: some words may carry significant historical or political meanings. Please be mindful of this and use alternatives.

## How do I know if particular terminology is OK or not? { #questions }

Apply the principles above. If you have any questions, you can reach out to `v8-dev@googlegroups.com`.

## What are examples of terminology to be avoided? { #examples }

This list is NOT meant to be comprehensive. It contains a few examples that people have run into frequently.

:::table-wrapper
| Term      | Suggested alternatives                                        |
| --------- | ------------------------------------------------------------- |
| master    | primary, controller, leader, host                             |
| slave     | replica, subordinate, secondary, follower, device, peripheral |
| whitelist | allowlist, exception list, inclusion list                     |
| blacklist | denylist, blocklist, exclusion list                           |
| insane    | unexpected, catastrophic, incoherent                          |
| sane      | expected, appropriate, sensible, valid                        |
| crazy     | unexpected, catastrophic, incoherent                          |
| redline   | priority line, limit, soft limit                              |
:::

## What if I am interfacing with something that violates this policy? { #violations }

This circumstance has come up a few times, particularly for code implementing specifications. In these circumstances, differing from the language in the specification may interfere with the ability to understand the implementation. For these circumstances, we suggest one of the following, in order of decreasing preference:

1. If using alternate terminology doesn’t interfere with understanding, use alternate terminology.
1. Failing that, do not propagate the terminology beyond the layer of code that is performing the interfacing. Where necessary, use alternative terminology at the API boundaries.
