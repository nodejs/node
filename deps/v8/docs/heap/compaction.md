# Compaction and Evacuation in V8

While [Marking and Sweeping](marking-and-sweeping.md) reclaims dead objects by adding their memory ranges to free lists, repeated allocations and deallocations of varying sizes leave `OldSpace`, `CodeSpace`, and `TrustedSpace` pages fragmented. When fragmentation is high, a 256KB `NormalPage` may contain only a few kilobytes of live objects scattered across the page, preventing large contiguous LAB allocations and wasting physical RAM.

V8 resolves fragmentation during Major GC (`MarkCompactCollector` in `src/heap/mark-compact.h`) via **selective page evacuation**: rather than compacting the entire heap on every collection, V8 selects only the most fragmented pages as **Evacuation Candidates** and copies their live objects onto fresh pages in parallel.

---

## 1. Selecting Evacuation Candidates (`StartCompaction`)

Before marking begins (during `MarkCompactCollector::StartCompaction`), V8 evaluates `NormalPage`s in `OldSpace`, `CodeSpace`, and `TrustedSpace` to choose which pages to evacuate:

1. **Fragmentation Heuristic (`FreeList` / `LiveBytes`)**:
   * Pages with the lowest live byte count (highest wasted/free space) are ranked first, up to a configurable compaction budget (`v8_flags.max_compaction_pages` or memory reducer pressure).
   * Pages that cannot move (such as `LargePage`s, pages containing pinned objects from Conservative Stack Scanning, or black-allocated pages) are excluded.
2. **Marking the Page as an Evacuation Candidate**:
   * Selected pages have `MutablePage::SetFlagNonExecutable(MemoryChunk::EVACUATION_CANDIDATE)` and `POINTERS_TO_HERE_ARE_INTERESTING` set on their `MemoryChunk` / `MutablePage`.
   * Their free lists are detached so no new allocations can land on an evacuation candidate during incremental marking.
3. **Recording Incoming Slots During Marking (`OLD_TO_OLD`, `TRUSTED_TO_TRUSTED`, `TRUSTED_TO_CODE`)**:
   * Because only evacuation candidates will move during the major GC pause, V8 only needs to track pointers whose **target** resides on an `EVACUATION_CANDIDATE` page.
   * Whenever the marker visits a slot `host->slot` pointing to an object `target` on an `EVACUATION_CANDIDATE` page (or when a write barrier fires for a store to an evacuation candidate), the collector records `host->slot` in the **host page's** remembered set:
     * `OLD_TO_OLD` (`SlotSet` / `TypedSlotSet`) for standard tagged pointers,
     * `TRUSTED_TO_TRUSTED` for `ProtectedTaggedMember` pointers between `TrustedObject`s,
     * `TRUSTED_TO_CODE` for pointers from `Code` objects in `TrustedSpace` to `InstructionStream` objects in `CodeSpace`.

---

## 2. Parallel Evacuation (`EvacuatePagesInParallel`)

During the atomic pause (after marking completes), `MarkCompactCollector::Evacuate` spawns parallel `Evacuator` worker tasks (`src/heap/mark-compact.cc`):

1. **Thread-Local `CompactionSpace`s**:
   * Each worker thread owns a `CompactionSpaceCollection` containing private `CompactionSpace` instances for `OLD_SPACE`, `CODE_SPACE`, `TRUSTED_SPACE`, and `SHARED_SPACE`.
   * Workers allocate destination memory inside their private `CompactionSpace` LABs without locking, merging the filled pages back into the main spaces once evacuation finishes.
2. **Copying Live Objects (`LiveObjectVisitor`)**:
   * A worker claims an `EVACUATION_CANDIDATE` page and iterates all marked (black) objects on the page using its `MarkingBitmap`.
   * For each live object, the worker allocates an equal-sized block in the corresponding `CompactionSpace`, copies the object's raw bytes (`Heap::CopyBlock`), and calls `RecordMigratedSlotVisitor` on the new copy:
     * If the evacuated object itself contains pointers to young objects, shared objects, other evacuation candidates, or movable `ExternalPointerTable` entries, those slot addresses are re-recorded on the **new destination page's** remembered sets (`OLD_TO_NEW`, `OLD_TO_OLD`, `OLD_TO_SHARED`, etc.).
3. **Installing the Forwarding Pointer (`MapWord::FromForwardingAddress`)**:
   * After copying `src` to `dst`, the worker overwrites `src`'s map word at offset `0` with a forwarding pointer to `dst`:
     ```cpp
     src->set_map_word_forwarded(dst, kRelaxedStore);
     ```
   * A forwarding `MapWord` clears the lowest 2 tag bits (`kForwardingTag = 0`, whereas a valid `Map` pointer always has `kHeapObjectTag = 1` in its lowest 2 bits).
4. **Aborted Evacuation & Pinning (`NormalPage::AbortEvacuation` & `PostProcessAbortedEvacuationCandidates`)**:
   * If Conservative Stack Scanning discovers a direct pointer from the C++ stack into an `EVACUATION_CANDIDATE` page, or if a worker runs out of memory while evacuating a page, evacuation for that page is **aborted** via `NormalPage::AbortEvacuation()` (setting `EvacuationWasAbortedField` on `BasePage`).
   * Before pointers are updated, `MarkCompactCollector::PostProcessAbortedEvacuationCandidates` walks any aborted pages to re-record slots for remaining un-evacuated live objects, and `ReleaseEvacuationCandidates` clears `EVACUATION_CANDIDATE` and schedules the page for normal sweeping in place.

---

## 3. Updating Pointers (`UpdatePointersAfterEvacuation`)

Once all evacuation candidates have been copied and their old object headers contain forwarding pointers, parallel **`PointersUpdatingJob`** tasks update every reference that still points to an old address:

```
Before Pointer Updating:
  Host Object Slot ------> [Old Object on Evacuation Candidate]
                           | map_word_: Forwarding Ptr (tag 00) |
                                              |
                                              v
                           [New Copy on CompactionSpace Page]
                           | map_word_: Real Map Ptr   (tag 01) |

After Pointer Updating:
  Host Object Slot ------> [New Copy on CompactionSpace Page]
```

Pointer updating visits:
1. **Roots**: `Isolate` roots, `HandleScope` slots, `GlobalHandles`, `TracedHandles`, and interpreter/deoptimizer stack frames.
2. **Remembered Sets on Non-Evacuated Pages**:
   * Iterates every slot recorded in `OLD_TO_OLD` (`SlotSet` and `TypedSlotSet`), `OLD_TO_NEW`, `TRUSTED_TO_TRUSTED`, and `TRUSTED_TO_CODE`.
   * For each slot, loads the target `HeapObject`, checks `target->map_word(kRelaxedLoad).IsForwardingAddress()`, and if true, replaces the slot with `map_word.ToForwardingAddress(target)`.
3. **Sandbox Indirect Pointer Tables (`TrustedPointerTable` & `JSDispatchTable`)**:
   * When an `ExposedTrustedObject` in `TrustedSpace` is evacuated, its `self_indirect_pointer` handle gives its exact index in the `TrustedPointerTable`. Evacuation updates the raw pointer stored in that `TrustedPointerTable` entry (and `JSDispatchTable` entries for `Code`), so in-sandbox objects holding `TrustedPointerHandle`s or `JSDispatchHandle`s automatically resolve to the new address without scanning in-sandbox fields.
4. **Releasing Evacuated Pages**:
   * After all pointers have been updated, the original `EVACUATION_CANDIDATE` pages contain no remaining live references and are released in bulk back to the `MemoryAllocator`.

---

## 4. External Pointer Table (EPT) Compaction (`src/sandbox/external-pointer-table.h`)

Under the V8 Sandbox, off-heap C++ pointers are stored in the out-of-sandbox **`ExternalPointerTable`**, which can also become fragmented as objects die. Major GC can compact the old `ExternalPointerTable::Space`:
1. **`StartCompactingIfNeeded`**: If free capacity in `old_ept_space` exceeds a threshold, an evacuation threshold segment is chosen; any EPT entry allocated above that index will be relocated to a free entry below the threshold.
2. **Recording Evacuation Entries During Marking**: When a marker visits an `ExternalPointerSlot` whose handle index is above the evacuation threshold, `Mark` allocates a new destination EPT entry below the threshold and stores the `slot` address inside that destination entry as an **Evacuation Entry** (`kExternalPointerEvacuationEntryTag`).
3. **`SweepAndCompact`**: At the end of the GC pause, EPT sweeping copies the payload from the old entry into the new destination entry and overwrites the in-sandbox `ExternalPointerSlot` with the new `ExternalPointerHandle` (unless the slot was mutated in the meantime, in which case `WriteBarrier::ForExternalPointer` invalidated the evacuation entry).
