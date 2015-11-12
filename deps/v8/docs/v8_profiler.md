# Introduction

V8 has built-in sample based profiling. Profiling is turned off by default, but can be enabled via the --prof command line option. The sampler records stacks of both JavaScript and C/C++ code.

# Build
Build the d8 shell following the instructions at [BuildingWithGYP](BuildingWithGYP.md).


# Command Line
To start profiling, use the `--prof` option.  When profiling, V8 generates a `v8.log` file which contains profiling data.

Windows:
```
build\Release\d8 --prof script.js
```

Other platforms (replace "ia32" with "x64" if you want to profile the x64 build):
```
out/ia32.release/d8 --prof script.js
```

# Process the Generated Output

Log file processing is done using JS scripts running by the d8 shell. For this to work, a `d8` binary (or symlink, or `d8.exe` on Windows) must be in the root of your V8 checkout, or in the path specified by the environment variable `D8_PATH`. Note: this binary is just used to process the log, but not for the actual profiling, so it doesn't matter which version etc. it is.

Windows:
```
tools\windows-tick-processor.bat v8.log
```

Linux:
```
tools/linux-tick-processor v8.log
```

Mac OS X:
```
tools/mac-tick-processor v8.log
```

## Snapshot-based VM build and builtins reporting

When a snapshot-based VM build is being used, code objects from a snapshot that don't correspond to functions are reported with generic names like _"A builtin from the snapshot"_, because their real names are not stored in the snapshot. To see the names the following steps must be taken:

  * `--log-snapshot-positions` flag must be passed to VM (along with `--prof`); this way, for deserialized objects the `(memory address, snapshot offset)` pairs are being emitted into profiler log;

  * `--snapshot-log=<log file from mksnapshot>` flag must be passed to the tick processor script; a log file from the `mksnapshot` program (a snapshot log) contains address-offset pairs for serialized objects, and their names; using the snapshot log, names can be mapped onto deserialized objects during profiler log processing; the snapshot log file is called `snapshot.log` and resides alongside with V8's compiled files.

An example of usage:
```
out/ia32.release/d8 --prof --log-snapshot-positions script.js
tools/linux-tick-processor --snapshot-log=out/ia32.release/obj.target/v8_snapshot/geni/snapshot.log v8.log
```

# Programmatic Control of Profiling
If you would like to control in your application when profile samples are collected, you can do so.

First you'll probably want to use the `--noprof-auto` command line switch which prevents the profiler from automatically starting to record profile ticks.

Profile ticks will not be recorded until your application specifically invokes these APIs:
  * `V8::ResumeProfiler()` - start/resume collection of data
  * `V8::PauseProfiler()` - pause collection of data

# Example Output

```
Statistical profiling result from benchmarks\v8.log, (4192 ticks, 0 unaccounted, 0 excluded).

 [Shared libraries]:
   ticks  total  nonlib   name
      9    0.2%    0.0%  C:\WINDOWS\system32\ntdll.dll
      2    0.0%    0.0%  C:\WINDOWS\system32\kernel32.dll

 [JavaScript]:
   ticks  total  nonlib   name
    741   17.7%   17.7%  LazyCompile: am3 crypto.js:108
    113    2.7%    2.7%  LazyCompile: Scheduler.schedule richards.js:188
    103    2.5%    2.5%  LazyCompile: rewrite_nboyer earley-boyer.js:3604
    103    2.5%    2.5%  LazyCompile: TaskControlBlock.run richards.js:324
     96    2.3%    2.3%  Builtin: JSConstructCall
    ...

 [C++]:
   ticks  total  nonlib   name
     94    2.2%    2.2%  v8::internal::ScavengeVisitor::VisitPointers
     33    0.8%    0.8%  v8::internal::SweepSpace
     32    0.8%    0.8%  v8::internal::Heap::MigrateObject
     30    0.7%    0.7%  v8::internal::Heap::AllocateArgumentsObject
    ...


 [GC]:
   ticks  total  nonlib   name
    458   10.9%

 [Bottom up (heavy) profile]:
  Note: percentage shows a share of a particular caller in the total
  amount of its parent calls.
  Callers occupying less than 2.0% are not shown.

   ticks parent  name
    741   17.7%  LazyCompile: am3 crypto.js:108
    449   60.6%    LazyCompile: montReduce crypto.js:583
    393   87.5%      LazyCompile: montSqrTo crypto.js:603
    212   53.9%        LazyCompile: bnpExp crypto.js:621
    212  100.0%          LazyCompile: bnModPowInt crypto.js:634
    212  100.0%            LazyCompile: RSADoPublic crypto.js:1521
    181   46.1%        LazyCompile: bnModPow crypto.js:1098
    181  100.0%          LazyCompile: RSADoPrivate crypto.js:1628
    ...
```

# Timeline plot
The timeline plot visualizes where V8 is spending time. This can be used to find bottlenecks and spot things that are unexpected (for example, too much time spent in the garbage collector). Data for the plot are gathered by both sampling and instrumentation. Linux with gnuplot 4.6 is required.

To create a timeline plot, run V8 as described above, with the option `--log-timer-events` additional to `--prof`:
```
out/ia32.release/d8 --prof --log-timer-events script.js
```

The output is then passed to a plot script, similar to the tick-processor:
```
tools/plot-timer-events v8.log
```

This creates `timer-events.png` in the working directory, which can be opened with most image viewers.

# Options
Since recording log output comes with a certain performance overhead, the script attempts to correct this using a distortion factor. If not specified, it tries to find out automatically. You can however also specify the distortion factor manually.
```
tools/plot-timer-events --distortion=4500 v8.log
```

You can also manually specify a certain range for which to create the plot or statistical profile, expressed in milliseconds:
```
tools/plot-timer-events --distortion=4500 --range=1000,2000 v8.log
tools/linux-tick-processor --distortion=4500 --range=1000,2000 v8.log
```

# HTML 5 version
Both statistical profile and timeline plot are available [in the browser](http://v8.googlecode.com/svn/branches/bleeding_edge/tools/profviz/profviz.html). However, the statistical profile lacks C++ symbol resolution and the Javascript port of gnuplot performs an order of magnitude slower than the native one.