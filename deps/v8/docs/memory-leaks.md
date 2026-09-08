---
title: 'Investigating memory leaks'
description: 'This document provides guidance on investigating memory leaks in V8.'
---
If you’re investigating a memory leak and wonder why an object is not garbage-collected, you can use `%DebugTrackRetainingPath(object)` to print the actual retaining path of the object on each GC.

This requires `--allow-natives-syntax --track-retaining-path` run-time flags and works both in release and debug modes. More info in the CL description.

Consider the following `test.js`:

```js
function foo() {
  const x = { bar: 'bar' };
  %DebugTrackRetainingPath(x);
  return () => { return x; }
}
const closure = foo();
gc();
```

Example (use debug mode or `v8_enable_object_print = true` for much more verbose output):

```bash
$ out/x64.release/d8 --allow-natives-syntax --track-retaining-path --expose-gc test.js
#################################################
Retaining path for 0x245c59f0c1a1:

^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Distance from root 6: 0x245c59f0c1a1 <Object map = 0x2d919f0d729>

^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Distance from root 5: 0x245c59f0c169 <FixedArray[5]>

^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Distance from root 4: 0x245c59f0c219 <JSFunction (sfi = 0x1fbb02e2d7f1)>

^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Distance from root 3: 0x1fbb02e2d679 <FixedArray[5]>

^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Distance from root 2: 0x245c59f0c139 <FixedArray[4]>

^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Distance from root 1: 0x1fbb02e03d91 <FixedArray[279]>

^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Root: (Isolate)
-------------------------------------------------
```

## Debugger support

While in a debugger session (e.g. `gdb`/`lldb`), and assuming you passed the above flags to the process (i.e. `--allow-natives-syntax --track-retaining-path`), you may be able to `print isolate->heap()->PrintRetainingPath(HeapObject*)` on an object of interest.
