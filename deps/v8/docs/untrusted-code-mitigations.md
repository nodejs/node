---
title: 'Untrusted code mitigations'
description: 'If you embed V8 and run untrusted JavaScript code, enable V8’s mitigations to help protect against speculative side-channel attacks.'
---
In early 2018, researchers from Google’s Project Zero disclosed [a new class of attacks](https://googleprojectzero.blogspot.com/2018/01/reading-privileged-memory-with-side.html) which [exploit](https://security.googleblog.com/2018/01/more-details-about-mitigations-for-cpu_4.html) speculative execution optimizations used by many CPUs. Because V8 uses optimizing JIT compilers (TurboFan, Maglev) to make JavaScript run quickly, in certain circumstances it is vulnerable to the side-channel attacks described in the disclosure.

## Nothing changes if you execute only trustworthy code

If your product only uses an embedded instance of V8 to execute JavaScript or WebAssembly code that is entirely under your control, then your usage of V8 is likely unaffected by the Speculative Side-Channel Attacks (SSCA) vulnerability. A Node.js instance running only code that you trust is one such unaffected example.

In order to take advantage of the vulnerability, an attacker has to execute carefully crafted JavaScript or WebAssembly code in your embedded environment. If, as a developer, you have complete control over the code executed in your embedded V8 instance, then that is very unlikely to be possible. However, if your embedded V8 instance allows arbitrary or otherwise untrustworthy JavaScript or WebAssembly code to be downloaded and executed, or even generates and subsequently executes JavaScript or WebAssembly code that isn’t fully under your control (e.g. if it uses either as a compilation target), you may need to consider mitigations.

## If you do execute untrusted code…

### Use a modern version of V8 and enable mitigations

Mitigations for this class of attack have been available in V8 since 2018. Using a current, supported version of V8 is strongly advised.

V8 includes a flag to help provide protection against SSCA vulnerabilities. This flag, called `--untrusted-code-mitigations`, is enabled by default at runtime through a build-time GN flag called `v8_untrusted_code_mitigations`.

These mitigations include:

- Masking of addresses before memory accesses in WebAssembly and asm.js to ensure that speculatively executed memory loads cannot access memory outside of the WebAssembly and asm.js heaps.
- Masking of the indices in JIT code used to access JavaScript arrays and strings in speculatively executed paths to ensure speculative loads cannot be made with arrays and string to memory addresses that should not accessible to JavaScript code.

Embedders should be aware that these mitigations may come with a performance trade-off. The actual impact depends significantly on your workload. For workloads such as Speedometer the impact is negligible but for more extreme computational workloads it can be as much as 15%. If you fully trust the JavaScript and WebAssembly code that your embedded V8 instance executes, you may choose to disable these JIT mitigations by specifying the flag `--no-untrusted-code-mitigations` at runtime.

Note that V8 defaults to disabling these mitigations on platforms where it is assumed the embedder will use process isolation, such as platforms where Chromium uses site isolation.

### The V8 Sandbox

For modern versions of V8, the [V8 Sandbox](/docs/sandbox) is a significant security feature designed to prevent memory corruption within the V8 heap from leading to arbitrary code execution or access to memory outside the sandbox. While primarily focused on memory safety, it also provides a robust boundary for untrusted code execution.

### Sandbox untrusted execution in a separate process

If you execute untrusted JavaScript and WebAssembly code in a separate process from any sensitive data, the potential impact of SSCA is greatly reduced. Through process isolation, SSCA attacks are only able to observe data that is sandboxed inside the same process along with the executing code, and not data from other processes.

### Consider tuning your offered high-precision timers

A high-precision timer makes it easier to observe side channels in the SSCA vulnerability. If your product offers high-precision timers that can be accessed by untrusted JavaScript or WebAssembly code, consider making these timers more coarse or adding jitter to them.
