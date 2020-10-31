# Running perfetto in detached mode

This document describes the `--detach` and `--attach` advanced operating modes
of the `perfetto` cmdline client.

The use of `--detach` and `--attach` is highly discouraged because of the risk
of leaking tracing sessions and accidentally leaving tracing on for arbitrarily
long periods of time.

If what you are looking for is just a way to grab a trace in background (e.g.,
while the USB cable / adb is disconnected) from the adb shell simply use
`--background`.

Use case
--------
By default the tracing service `traced` keeps the lifetime of a tracing session
attached to the lifetime of the `perfetto` cmdline client that started it.
This means that a `killall perfetto` or `kill $PID_OF_PERFETTO` is sufficient
to guarantee that the tracing session is stopped.  
There are rare occasions when this is undesirable.

The use case this has been designed for is the Traceur app (on-device tracing
UI for Android).  
When required by the user, Traceur needs to enable tracing in the background,
possibly for very long periods of time.
Because Traceur is not a persistent service (and even if it was, it could be
still low-memory-killed), it cannot just use `--background`. This is
because the Android framework kills any other process in the same process group
when tearing down an app/service, and this would including killing forked
`perfetto` client obtained via `--background`.

Operation
---------
`--detach=key` decouples the lifetime of the cmdline client from the lifetime
of the tracing session.

The `key` argument is an arbitrary string passed by the client to later
re-identify the session using `--attach=key`.

Once detached, the cmdline client will exit (without forking any bg process) and
the `traced` service will keep the tracing session alive.  
Because of the exit, a client that wants to use `--detach` needs to set the
[`write_into_file`](long-traces.md) flag in the trace config, which transfers
the output trace file descriptor to the service (see the [examples](#examples)
section).

A detached session will run until either:

- The session is later re-attached and stopped.
- The time limit specified by the `duration_ms` argument in the trace config
  is reached.

`--attach=key` re-couples the lifetime of a new cmdline client invocation with
an existing tracing session identified by `key`.
For security reasons the service allows a client to re-attach to a tracing
session only if the Unix UID of the re-attaching client matches the UID of the
client that originally started the session and detached.

Overall `--attach=key` makes the `perfetto` cmdline client behave as if it was
never detached. This means that:

- sending a `SIGKILL` (or Ctrl-C) to the client will gracefully stop the tracing
  session.
- If the `duration_ms` time limit is hit, the client will be informed by the
  service and exit soon after.

When re-attaching it is possible to also specify a further `--stop` argument.
`--stop` will gracefully terminate the tracing session immediately after
re-attaching (This is to avoid a race where SIGKILL is sent too early, before
the client gets a chance to attach or even register the signal handler).

No other cmdline argument other than `--stop` can be passed when using
`--attach`.

`--is_detached=key` can be used to check whether a detached session is running.
The cmdline client will return quickly after the invocation with the following
exit code:

- 0 if the session identified by `key` exists and can be re-attached.
- 1 in case of a general error (e.g. wrong cmdline, cannot reach the service).
- 2 if no detached session with the given `key` is found.

Examples
--------

### Capturing a long trace in detached mode

```bash
echo '
write_into_file: true
# Long tracing mode, periodically flush the trace buffer into the trace file.
file_write_period_ms: 5000

buffers {
  # This buffer needs to be big enough just to hold data between two consecutive
  # |file_write_period|s (5s in this examples).
  size_kb: 16384
}

data_sources {
  config {
    name: "linux.ftrace"
    ftrace_config {
      ftrace_events: "sched_switch"
    }
  }
}
' | perfetto -c - --txt --detach=session1 -o /data/misc/perfetto-traces/trace

sleep 60

perfetto --attach=session1 --stop
# At this point the trace file is fully flushed into
# /data/misc/perfetto-traces/trace.
```

### Start in detached ring-buffer mode. Later stop and save the ring buffer

```bash
echo '
write_into_file: true

# Specify an arbitrarily long flush period. Practically this means: never flush
# unless trace is stopped.
# TODO(primiano): an explicit no_periodic_flushes argument would be nicer. Maybe
# we could repurpose the 0 value?
file_write_period_ms: 1000000000

buffers {
  # This will be the size of the final trace.
  size_kb: 16384
}

data_sources {
  config {
    name: "linux.ftrace"
    ftrace_config {
      ftrace_events: "sched_switch"
    }
  }
}
' | perfetto -c - --txt --detach=session2 -o /data/misc/perfetto-traces/trace

# Wait for user input, or some critical event to happen.

perfetto --attach=session2 --stop

# At this point the trace file is saved into
# /data/misc/perfetto-traces/trace.
```

### Start tracing with a time limit. Later re-attach and wait for the end

```bash
echo '
duration_ms: 10000
write_into_file: true

buffers {
  size_kb: 16384
}

data_sources {
  config {
    name: "linux.ftrace"
    ftrace_config {
      ftrace_events: "sched_switch"
    }
  }
}
' | perfetto -c - --txt --detach=session3 -o /data/misc/perfetto-traces/trace

sleep 3
perfetto --attach=session3
# The cmdline client will stay up for 7 more seconds and then terminate.
```
