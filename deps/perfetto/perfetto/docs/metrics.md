Writing Perfetto-based metrics
=============

Contents
---------
1. Background
2. The Perfetto Metrics Platform
3. Writing your first metric - step by step
4. Breaking down and composing metrics (TBD)
5. Adding a new metric or editing an existing metric (TBD)
6. Running a metric over a set of traces (TBD)
7. Metrics platform as an API (TBD)

Background
---------
Using traces allows computation of reproducible metrics in a wide range
of situations; examples include benchmarks, lab tests and on
large corpuses of traces. In these cases, these metrics allow for direct
root-causing when a regression is detected.

The Perfetto Metrics Platform
----------
The metrics platform (powered by the
[trace processor](trace-processor.md)) allows metrics authors to write
SQL queries to generate metrics in the form of protobuf messages or proto text.

We strongly encourage all metrics derived on Perfetto traces to be added to the
Perfetto repo unless there is a clear usecase (e.g. confidentiality) why these
metrics should not be publicly available.

In return for upstreaming metrics, authors will have first class support for
running metrics locally and the confidence that their metrics will remain stable
as trace processor is developed.

For example, generating the full (human readable) set of Android memory
metrics on a trace is as simple as:
```shell
trace_processor_shell --run-metrics android_mem <trace>
```

As well as scaling upwards while developing from running on a single trace
locally to running on a large set of traces, the reverse is also very useful.
When an anomaly is observed in the metrics of a lab benchmark, you can simply
download a representative trace and run the same metric locally in shell.

Since the same code is running locally and remotely, you can be confident in
reproducing the issue and use the power of trace processor and/or the Perfetto
UI to identify the problem!

Writing your first metric: A Step by Step Guide
----------
To begin, all you need is some familiarity with SQL and you're ready to start!

Suppose that want a write a metric which computes the CPU time for every process
in the trace and lists the names of the top 5 processes (by CPU time)
and the number of threads which were associated with those processes over its
lifetime.

*Note:*
* If you want to jump straight to the code, at the end of this guide, your
workspace should look something like this [GitHub gist](https://gist.github.com/tilal6991/c221cf0cae17e298dfa82b118edf9080). See Step 0 and 4
below as to where to get trace processor and how to run it to output the
metrics.

### Step 0
As a setup step, you'll want to create a folder to act as a scratch workspace;
this folder will be referred to using the env variable `$WORKSPACE` in Step 4.

The other thing you'll need is trace processor shell. You can download this
[here](https://get.perfetto.dev/trace_processor) or you can build from source
using the instructions [here](trace-processor.md). Whichever method is
chosen, $TRACE_PROCESSOR env variable will be used to refer to the location of
the binary in Step 4.

### Step 1
As all metrics in the metrics platform are defined using protos, the metric
needs to be strctured as a proto. For this metric, there needs to be some notion
of a process name along with its CPU time and number of threads.

Starting off, in a file named `top_five_processes.proto` in our workspace,
let's create a basic proto message called ProcessInfo with those three fields:
```protobuf
message ProcessInfo {
  optional string process_name = 1;
  optional uint64 cpu_time_ms = 2;
  optional uint32 num_threads = 3;
}
```

Next up is a wrapping message which will hold the repeated field containing
the top 5 processes.
```protobuf
message TopProcesses {
  repeated ProcessInfo process_info = 1;
}
```

Finally, let's define an extension to the root proto for all metrics -
the
[TraceMetrics](https://android.googlesource.com/platform/external/perfetto/+/HEAD/protos/perfetto/metrics/metrics.proto#39)
proto).
```protobuf
extend TraceMetrics {
  optional TopProcesses top_processes = 450;
}
```
Adding this extension field allows trace processor to link the newly defined
metric to the `TraceMetrics` proto.

*Notes:*
* The field ids 450-500 are reserved for local development so you can use
any of them as the field id for the extension field.
* The choice of field name here is important as the SQL file and the final
table generated in SQL will be based on this name.

Putting everything together, along with some boilerplate header information
gives:
```protobuf
syntax = "proto2";

package perfetto.protos;

import "protos/perfetto/metrics/metrics.proto";

message ProcessInfo {
  optional string process_name = 1;
  optional int64 cpu_time_ms = 2;
  optional uint32 num_threads = 3;
}

message TopProcesses {
  repeated ProcessInfo process_info = 1;
}

extend TraceMetrics {
  optional TopProcesses top_processes = 450;
}
```

### Step 2
Let's write the SQL to generate the table of the top 5 processes ordered
by the sum of the CPU time they ran for and the number of threads which were
associated with the process. The following SQL should be to a file called
`top_five_processes.sql` in your workspace:
```sql
CREATE VIEW top_five_processes_by_cpu
SELECT
  process.name as process_name,
  CAST(SUM(sched.dur) / 1e6 as INT64) as cpu_time_ms,
  COUNT(DISTINCT utid) as num_threads
FROM sched
INNER JOIN thread USING(utid)
INNER JOIN process USING(upid)
GROUP BY process.name
ORDER BY cpu_time_ms DESC
LIMIT 5;
```
Let's break this query down:
1. The first table used is the `sched` table. This contains all the
   scheduling data available in the trace. Each scheduling "slice" is associated
   with a thread which is uniquely identified in Perfetto traces using its
   `utid`. The two pieces of information which needed from the sched table
   is the `dur` - short for duration, this is the amount of time the slice
   lasted - and the `utid` which will be use to join with the thread table.
2. The next table is the thread table. This gives us a lot of information which
   are not particularly interested (including its thread name) but it does give
   us the `upid`. Similar to `utid`, `upid` is the unique identifier for a
   process in a Perfetto trace. In this case, `upid` will refer to the process
   which hosts the thread given by `utid`.
3. The final table is the process table. This gives the name of the
   process associated with the original sched slice.
4. With the process, thread and duration for each sched slice, all the slices
   for a single processes are collected and their durations summed to get the
   CPU time (dividing by 1e6 as sched's duration is in nanoseconds) and count
   the number of distinct threads.
5. Finally, we order by the cpu time and take limit to the top 5.

### Step 3
Now that the result of the metric has been expressed as an SQL table, it needs
to be converted a proto. The metrics platform has built-in support for emitting
protos using SQL functions; something which is used extensively in this step.

Let's look at how it works for our table above.
```sql
CREATE VIEW top_processes_output AS
SELECT TopProcesses(
  'process_info', (
    SELECT RepeatedField(
      ProcessInfo(
        'process_name', process_name,
        'cpu_time_ms', cpu_time_ms,
        'num_threads', num_threads
      )
    )
    FROM top_five_processes_by_cpu
  )
);
```
Let's break this down again:
1. Starting from the inner-most SELECT statement, there is
   what looks like a function call to the ProcessInfo function; in face this is
   no conincidence. For each proto that the metrics platform knows about,
   it generates a SQL function with the same name as the proto. This function
   takes key value pairs with the key as the name of the proto field to fill
   and the value being the data to store in the field. The output is the proto
   created by writing the fields described in the function! (*)

   In this case, this function is called once for each row in
   the `top_five_processes_by_cpu` table. The output of will be the fully filled
   ProcessInfo proto.

   The call to the `RepeatedField` function is the most interesting part and
   also the most important. In technical terms, `RepeatedField` is an aggregate
   function; practically, this means that it takes a full table of values and
   generates a single array which contains all the values passed to it.

   Therefore, the output of this whole SELECT statement is an array of
   5 ProcessInfo protos.
2. Next is creation of the `TopProcesses` proto. By now, the syntax should
   already feel somewhat familiar; the proto builder function is called
   to fill in the `process_info` field with the array of protos from the
   inner funciton.

   The output of this SELECT is a single `TopProcesses` proto containing
   the ProcessInfos as a repeated field.
3. Finally, the view is created. This view is specially named to allow the
   metrics platform to query it to obtain the root proto for each metric (in
   this case `TopProcesses`). See the note below as to the pattern behind
   this view's name.

(*) - side note: this is not strictly true. To type-check the protos, we
also return some metadata about the type of the proto but this is unimportant
for metric authors

*Note:*
* It is important that the views be named
  {name of TraceMetrics extension field}_output. This is the pattern used
  and expected by the metrics platform for all metrics.

And that's all the SQL we need to write! Our final file should look like so:
```sql
CREATE VIEW top_five_processes_by_cpu AS
SELECT
  process.name as process_name,
  CAST(SUM(sched.dur) / 1e6 as INT64) as cpu_time_ms,
  COUNT(DISTINCT utid) as num_threads
FROM sched
INNER JOIN thread USING(utid)
INNER JOIN process USING(upid)
GROUP BY process.name
ORDER BY cpu_time_ms DESC
LIMIT 5;

CREATE top_processes_output AS
SELECT TopProcesses(
  'process_info', (
    SELECT RepeatedField(
      ProcessInfo(
        'process_name', process_name,
        'cpu_time_ms', cpu_time_ms,
        'num_threads', num_threads
      )
    )
    FROM top_five_processes_by_cpu
  )
);
```

*Notes:*
* The name of the SQL file should be the same as the name of TraceMetrics
  extension field. This is to allow the metrics platform to associated the
  proto extension field with the SQL which needs to be run to generate it.

### Step 4
This is the last step and where we get to see the results of our work!

For this step, all we need is a one-liner, invoking trace processor
shell (see Step 0 for downloading it):
```shell
$TRACE_PROCESSOR --run-metrics $WORKSPACE/top_five_processes.sql $TRACE 2> /dev/null
```
(If you want a example trace to test this on, see the Notes section below.)

By passing the SQL file for the metric we want to compute, trace processor uses
the name of this file to both find the proto and also to figure out the name
of the output table for the proto and the name of the extension field for
`TraceMetrics`; this is why it was important to choose the names of these other
objects carefully.

*Notes:*
* If something doesn't work as intended, check that your workspace looks the
  same as the contents of this [GitHub gist](https://gist.github.com/tilal6991/c221cf0cae17e298dfa82b118edf9080).
* A good example trace for this metric is the Android example trace used by
  the Perfetto UI found [here](https://storage.googleapis.com/perfetto-misc/example_android_trace_30s_1)
* We're redirecting stderror to remove any noise from parsing the trace that
  trace processor generates.

If everything went successfully, you should see something like the following
(this is specifically the output for the Android example trace linked above):
```
[perfetto.protos.top_five_processes] {
  process_info {
    process_name: "com.google.android.GoogleCamera"
    cpu_time_ms: 15154
    num_threads: 125
  }
  process_info {
    process_name: "sugov:4"
    cpu_time_ms: 6846
    num_threads: 1
  }
  process_info {
    process_name: "system_server"
    cpu_time_ms: 6809
    num_threads: 66
  }
  process_info {
    process_name: "cds_ol_rx_threa"
    cpu_time_ms: 6684
    num_threads: 1
  }
  process_info {
    process_name: "com.android.chrome"
    cpu_time_ms: 5125
    num_threads: 49
  }
}
```

### Conclusion
That finishes the introductory guide to writing an metric using the Perfetto
metrics platform! For more information about where to go next, the following
links may be useful:
* To understand what data is available to you and how the SQL tables are
  structured see the [trace processor](trace-processor.md) docs.
* To see how you can use the RUN_METRIC function to extract common snippets of
  SQL and reuse them for writing bigger metrics, continue reading!
* To see how you can add your own metrics to the platform or edit an existing
  metric, continue reading!

Breaking down and composing metrics
----------
Coming soon!

Adding a new metric or editing an existing metric
----------
Coming soon!

Running a metric over a set of traces
----------
Coming soon!

Metrics platform as an API
----------
Coming soon!
