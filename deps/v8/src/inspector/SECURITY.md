# V8 Inspector Security Framework

This document outlines the security goals, boundaries, and threat model for the V8 Inspector (the Chrome DevTools Protocol implementation in V8).

## Evaluating Severity

The severity of a security vulnerability in the V8 Inspector is determined by the level of user interaction required to trigger it.

*   **High Severity:** Vulnerabilities that can be exploited with minimal user interaction, such as simply opening DevTools (establishing a CDP connection and starting a session) or those that trigger automatically upon connection.
*   **Low Severity:**
    *   Vulnerabilities requiring the user to perform a complex series of manual steps within the DevTools UI.
    *   Issues that assume the attacker already possesses an established CDP connection (e.g., via a malicious debugger extension or a compromised client).

## Reproductions and Proof-of-Concepts

To be considered a valid security vulnerability, a proof-of-concept (PoC) or reproduction **must** be reproducible using the `inspector-test` harness.

*   **Invalid Reproductions:** Reproductions using `d8 --enable-inspector` are generally considered **invalid**. `d8` processes CDP traffic synchronously, which can lead to behaviors—such as specific re-entrancy patterns—that do not exist in production environments like Chrome or the `inspector-test` harness.
*   **Rationale:** For example, re-entering the V8 Inspector while a CDP event is still on the stack is impossible in Chrome or `inspector-test`, but may appear possible in `d8` due to its synchronous nature.

## Security Goals and Boundaries

The V8 Inspector operates under the following security principles:

*   **Privileged Access:** Access to the Chrome DevTools Protocol is inherently privileged. While the V8 Inspector itself cannot enforce connection security, embedders must ensure that establishing a CDP connection requires explicit, non-trivial user authorization.
*   **Trusted vs. Untrusted CDP Clients:** Untrusted clients should only be granted access to CDP domains that can be strictly scoped to the specific frame or worker context group they are authorized to inspect. For example, the `HeapProfiler` domain is restricted because heap snapshots expose the memory layout and content of the entire V8 isolate, potentially leaking data from other contexts.
*   **Memory Corruption:** Memory corruption within the V8 Inspector or the V8 heap triggered via CDP is a high-priority issue. However, memory corruption within a page renderer via CDP is generally treated with the same severity as memory corruption via regular JavaScript.
*   **Data Access:** Providing access to data outside established security boundaries (e.g., local filesystem paths) is only permissible if it can be guaranteed that the user already has legitimate access to that data.
*   **Social Engineering:** Tricking a user into performing unsafe actions, such as executing malicious code via the console, is considered a UX or social engineering challenge rather than a protocol vulnerability.
*   **Inspector Detection:** Preventing inspected code from detecting the presence of the V8 Inspector is a non-goal. It is not considered a vulnerability if a script can determine whether it is being debugged or if a CDP session is active.

## Reporting Vulnerabilities

To report a security issue, please follow the [Chromium security bug reporting guidelines](https://www.chromium.org/Home/chromium-security/reporting-security-bugs/). When filing your report, please specify that the issue pertains to the V8 Inspector.
