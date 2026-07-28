# Why V8 Embraced CFG (Control-Flow Graph) in Maglev and Turboshaft

This document explains the technical reasons and benefits of using a traditional Control-Flow Graph (CFG) in V8's modern optimizing compilers, Maglev and Turboshaft, moving away from the Sea of Nodes (SoN) representation used in TurboFan.

## Historical Context: The Sea of Nodes Era

For many years, V8's TurboFan compiler used a **Sea of Nodes** (SoN) intermediate representation. In a Sea of Nodes, instructions are not bound to basic blocks; instead, they float freely based only on their true data, effect, and control dependencies.

The promise of Sea of Nodes was that by relaxing the schedule until the very end of the pipeline, the compiler could find better global optimizations. It was a powerful approach that allowed TurboFan to overcome the limitations of its predecessor, Crankshaft.

However, as the codebase grew and JavaScript evolved, the V8 team found that the Sea of Nodes approach introduced significant complexity and overhead that often outweighed its benefits for JavaScript workloads.

For a detailed account of the transition, see the v8.dev blog post [Land ahoy: leaving the Sea of Nodes](https://v8.dev/blog/leaving-the-sea-of-nodes).

## The Shift to CFG in Maglev and Turboshaft

With the development of **Maglev** (V8's mid-tier compiler) and **Turboshaft** (V8's top-tier compiler), V8 returned to a more traditional **Control-Flow Graph** (CFG) representation, but with modern design choices that address the shortcomings of past architectures.

Here is why CFG is the right choice for V8's current and future needs, applying to both tiers:

### 1. Explicit Control Flow Fits JavaScript Better

JavaScript is a highly dynamic language with frequent side effects, exceptions, and complex control flow. In a Sea of Nodes, representing these constraints requires complex chains of effect and control edges.

In a CFG, control flow is explicit. Basic blocks define a clear execution order. This makes it much easier for both Maglev and Turboshaft to:
- Reason about side effects and dependencies.
- Implement optimizations that rely on control flow structure (like loop optimizations).
- Avoid subtle bugs related to manual chain management in lowerings.

### 2. Faster Compilation and Support for Linear Passes

A key goal for both compilers is compilation speed. CFG naturally supports fast, linear-scan algorithms.

- **Linear Passes**: CFG allows for efficient linear-scan style optimizations and register allocation, avoiding the need to repeatedly visit nodes in a non-linear graph. Both Maglev and Turboshaft use linear scan register allocation, which is much simpler on a CFG.
- **Maglev's Direct Generation**: Maglev builds its graph in a single pass and generates code directly by iterating over blocks and nodes, avoiding complex scheduling phases.
- **Turboshaft's Copying Approach**: Turboshaft uses a copying phase approach rather than in-place mutation. This keeps nodes ordered linearly in memory, significantly reducing cache misses.

### 3. Improved Developer Velocity and Readability

A "sea of nodes" can quickly become a "soup of nodes," making it incredibly difficult for compiler engineers to inspect, debug, and understand the graph. This applies to any compiler tier.

A CFG is much more intuitive. It resembles standard code structure with blocks and branches, making it easier for developers of both Maglev and Turboshaft to:
- Visualize the graph in tools like Turbolizer.
- Debug optimization passes.
- Onboard new engineers to the compiler team.

### 4. Powerful and Predictable Optimizations

While Sea of Nodes promised global optimizations, in practice, it often made certain optimizations harder.

- **Loop Optimizations**: In a CFG, loop headers and bodies are explicitly defined, making loop peeling, unrolling, and invariant code motion much more straightforward.
- **Predictable Scheduling**: In Sea of Nodes, scheduling (assigning nodes to blocks) happened late and could sometimes produce unpredictable results or float operations too high. In a CFG, the schedule is maintained throughout, leading to more predictable behavior.

## Conclusion

The move to CFG in Maglev and Turboshaft is not a step backward, but a step forward. By adopting a structured CFG representation, V8 achieves massive improvements in compilation speed, codebase maintainability, and predictable optimizations across its modern optimizing compiler tiers.

---

## See Also
- [Turboshaft](turboshaft/compiler-turboshaft.md): Overview of the Turboshaft compiler.
- [Turboshaft Copying Approach](turboshaft/compiler-turboshaft-copying-approach.md): Details on Turboshaft's semi-space IR.
- [Land ahoy: leaving the Sea of Nodes](https://v8.dev/blog/leaving-the-sea-of-nodes) on v8.dev.
