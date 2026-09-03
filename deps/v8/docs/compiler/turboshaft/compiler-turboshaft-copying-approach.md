# Turboshaft's Copying Approach: Semi-Spaces for IR

This document explains a core design philosophy of the Turboshaft compiler: using a copying approach for graph transformations instead of in-place mutation.

## The Core Philosophy: Copies are Cheap

In traditional compilers, optimization passes often mutate the Intermediate Representation (IR) graph in-place. While this avoids copying overhead, it introduces significant complexity:
*   **Complex Bookkeeping**: In-place mutation requires carefully updating def-use chains, invalidating and recomputing analyses, and handling node replacement safely.
*   **Brittleness**: It is easy to leave the graph in an inconsistent state or violate invariants during complex transformations.

Turboshaft takes the opposite approach. It assumes that **graph copies are cheap** and that the graph changes so much between phases that a full copy is often more efficient and much simpler to implement.

## The "Semi-Spaces" Model

Similar to a semi-space garbage collector, a typical Turboshaft phase operates on two graphs:
1.  **Input Graph**: The read-only source graph for the phase.
2.  **Output Graph**: A brand new graph being constructed by the phase.

This approach is primarily implemented by `CopyingPhase` and `GraphVisitor` (see `src/compiler/turboshaft/copying-phase.h`).

A phase iterates linearly through the blocks and operations of the input graph. As it processes each operation, it uses the **Reducer Stack** to determine what should be emitted into the output graph. The output graph then becomes the input graph for the next phase (often achieved by swapping the graphs, see `Graph::SwapWithCompanion` in `src/compiler/turboshaft/graph.h`).

## Why is Copying Cheap in Turboshaft?

Turboshaft's IR was designed from the ground up to make this copying approach viable and fast:

### 1. Contiguous Memory Layout
Unlike Sea-of-Nodes (used in TurboFan), which uses a network of pointers between nodes scattered in memory, Turboshaft stores operations in a contiguous buffer called `OperationBuffer` (see `src/compiler/turboshaft/graph.h`).
*   Operations are allocated sequentially in a zone.
*   Iterating over the graph is a simple, cache-friendly linear scan of memory.

### 2. Compact Representation
Operations are compact. References to other operations are not pointers but small integer offsets (`OpIndex`). This keeps the memory footprint small, making full graph copies fit well within CPU caches.

### 3. Fast Allocation
Emitting an operation into the output graph is typically just an append operation to the end of the `OperationBuffer`, involving a few pointer increments and a store.

## Advantages of the Copying Approach

*   **Simplicity**: Reducers only need to worry about generating the *correct new code*, not fixing up old code or maintaining complex graph invariants.
*   **Immutability**: The input graph is immutable during a phase, making it safe to query without worrying about concurrent modifications or invalidation of analysis results.
*   **Natural Dead Code Elimination**: If an operation in the input graph is not needed (e.g., it was simplified away or determined to be unreachable), the reducer simply refrains from emitting anything to the output graph. It naturally disappears.

---

## See Also
-   [Turboshaft Reducers](compiler-turboshaft-reducers.md): How reducers compose to transform the graph during copying.
-   `src/compiler/turboshaft/graph.h`: Definition of `Graph` and `OperationBuffer`.
-   `src/compiler/turboshaft/copying-phase.h`: Implementation of the copying phase driver.
