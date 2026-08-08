---
name: v8-poc-classification
description: "Checks if a POC provided by some JS and d8 flags is a vulnerability or just a regular bug."
---

# Skill: V8 POC Classification

Use this skill to determine if a reported Proof-of-Concept (POC) crash is a security vulnerability or a regular bug.

## Workflow

When provided with a reproduction script (JS) and a set of `d8` flags that cause a crash, follow these steps to classify the bug:

### 1. Initial Verification
Reproduce the crash using the *provided* flags to confirm the bug exists.
- Command: `d8 <provided-flags> <poc.js>`

### 2. Check with Security POC Flag
Run the reproduction with the meta-flag `--run-as-security-poc`. This is the most important step.
- Command: `d8 --run-as-security-poc <provided-flags>  <poc.js>`
- **Result**: If the crash still occurs, it is highly likely a valid security vulnerability (**Type=Vulnerability**).

### 3. Isolate Required Flags (If needed)
If the crash stops reproducing with `--run-as-security-poc`, you need to find out which restriction prevents the crash. Try running with individual flags (e.g., `--disallow-unsafe-flags`, `--disallow-developer-only-features`, `--no-experimental`, or `--fuzzing`) to isolate the behavior.

Refer to [reproducing-bugs.md](../../../docs/security/reproducing-bugs.md) to understand the implications. In general, if a POC stops reproducing when any of these flags are set, it is likely not a security bug.
