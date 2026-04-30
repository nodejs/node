# Heap Compaction in V8

Compaction is a phase of the Major Garbage Collector (Mark-Sweep-Compact) used to resolve memory fragmentation in the Old Generation and other spaces.

## Overview

Over time, as objects are allocated and freed, the heap can become fragmented—full of small free spaces that are too small to satisfy new allocation requests. Compaction solves this by moving live objects closer together, creating large contiguous free spaces.

## The Compaction Pipeline

### 1. Identification of Evacuation Candidates
V8 does not compact the entire heap at once, as that would cause unacceptably long pause times. Instead, it identifies specific pages as "evacuation candidates" in `CollectEvacuationCandidates`:
*   **Thresholds**: V8 calculates a fragmentation threshold. Pages with free space exceeding this threshold are considered candidates.
*   **Heuristics**: To bound pause times, V8 limits the total size of objects to be evacuated (`max_evacuated_bytes`). It sorts candidate pages by the amount of **live bytes in ascending order** (i.e., pages with the most free space first). It then selects the candidates that fit within the limit, maximizing reclaimed space per byte moved.
*   **Heuristics calculation**: `ComputeEvacuationHeuristics` determines the target fragmentation and max evacuated bytes based on the mode (ReduceMemory, OptimizeForMemoryUsage, or regular latency-critical mode). In regular mode, it may use trace-based compaction speed to adaptively set the threshold.
*   **Spaces**: Compaction is performed on `OLD_SPACE`, `TRUSTED_SPACE`, and potentially `CODE_SPACE` and `SHARED_SPACE` (with specific conditions, e.g., not compacting shared space when CSS is enabled in certain modes).

### 2. Parallel Evacuation
The actual moving of objects happens in parallel to utilize multiple cores:
*   **Page Promotion**: For some pages in the new space (or if forced due to stack scanning limitations), V8 might promote the **entire page** to the old generation simply by updating metadata, without copying objects.
*   **Object Evacuation**: For pages that must be compacted, V8 iterates over live objects and copies them to newly allocated spaces in non-candidate pages.

### 3. Forwarding Addresses
When an object is moved to a new location:
*   V8 overwrites the original object's map word with a **forwarding address** pointing to the new location.
*   This forwarding address is crucial for updating pointers that still point to the old location.

### 4. Pointer Updating
After all objects have been moved, V8 must update all references to point to the new locations in `UpdatePointersAfterEvacuation`:
*   **Roots**: V8 scans all roots (stack, handles, etc.) and updates pointers to moved objects.
*   **Remembered Sets**: V8 processes remembered sets to update inter-object references across the heap. This includes:
    *   `OLD_TO_NEW`: Pointers from old to new generation.
    *   `OLD_TO_SHARED`: Pointers from non-shared to shared space.
    *   `OLD_TO_OLD`: Pointers between old generation pages, specifically recorded when the target points to an evacuation candidate.
    *   `TRUSTED_TO_CODE`: Pointers from trusted space to code space.
    *   `TRUSTED_TO_TRUSTED`: Pointers within trusted space.
    *   `TRUSTED_TO_SHARED_TRUSTED`: Pointers from trusted space to shared trusted space.

## Additional Compaction Areas
*   **External Pointer Tables**: With the V8 Sandbox enabled, external pointer tables (young, old, cpp_heap) are also compacted if needed, starting at the beginning of the marking phase in `StartMarking`.

## File Structure
*   `src/heap/mark-compact.cc`: Contains the implementation of the compaction logic, including candidate selection, evacuation, and pointer updating.
