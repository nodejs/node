# Running Perfetto

In order to run Perfetto and get a meaningful trace you need to build
(see [build instructions](build-instructions.md)) and run the following:

`traced`:
The unprivileged trace daemon that owns the log buffers and maintains
a registry of Producers and Consumers connected.

`traced_probes`:
The privileged daemon that has access to the Kernel tracefs
(typically mounted under `/sys/kernel/debug/tracing`). It drives
[Ftrace](https://source.android.com/devices/tech/debug/ftrace) and writes its
protobuf-translated contents into `traced`.

`perfetto`:
A command line utility client that drive the trace and save back
the results (either to a file or to [Android's Dropbox][dropbox])

Running from a standalone checkout (Linux, Mac or Android)
-------------------------------------------------------------
A convenience script allows to run Perfetto daemons (`traced`, `traced_probes`)
and the command line client (`perfetto`) in a tmux-based terminal:
```bash
CONFIG=ftrace.cfg OUT=out/default ./tools/tmux
```

The script will automatically serialize the trace config defined in the
`CONFIG` variable (e.g., [this](https://android.googlesource.com/platform/external/perfetto/+/master/test/configs/ftrace.cfg)) into a protobuf and setup the right paths.
Furthermore it will automatically rebuild if necessary.

It is possible to push binaries to, and run on, a remote target over ssh (even
when cross-compiling):
```bash
CONFIG=ftrace.cfg OUT=out/default SSH_TARGET=user@my-device-host ./tools/tmux
```

Running from an Android P+ in-tree build
----------------------------------------
Make sure that Perfetto daemons (`traced` / `traced_probes`) are running.
They are enabled by default on Pixel and Pixel 2 (walleye, taimen, marlin,
sailfish). On other devices start them manually by doing:
```bash
adb shell setprop persist.traced.enable 1
```

If this works you will see something like this in the logs:
```bash
$ adb logcat -s perfetto
perfetto: service.cc:45 Started traced, listening on /dev/socket/traced_producer /dev/socket/traced_consumer
perfetto: probes.cc:25 Starting /system/bin/traced_probes service
perfetto: probes_producer.cc:32 Connected to the service
```

At which point you can grab a trace by doing:

```bash
$ adb shell perfetto --config :test --out /data/misc/perfetto-traces/trace
```

For more advanced configurations see the [Trace Config](#trace-config) section.

*** aside
If the output file is not under `/data/misc/perfetto-traces`, tracing will
fail due to SELinux.
***

*** aside
For security reasons the trace file is written with 0600 (rw-------) permissions
and owned by shell. On rooted (`userbuild`) devices it is possible to just
`adb pull` the file after `adb root`. On `user` devices instead, in order to get
the trace out of the device, do the following:
`adb shell cat /data/misc/perfetto-traces/trace > ~/trace`
***

Trace config
------------
`--config :test` uses a hard-coded test trace config. It is possible to pass
an arbitrary trace config. See instructions in the
[trace config](trace-config.md) page.

Trace UI
--------
For building the trace UI see the [build instructions](build-instructions.md)
page. To run the UI using your local build:

```
$ ui/run-dev-server out/[your_build_dir]
```

Documentation
-------------
To run the documentation server using your local build:

```
$ make -C docs test
```
You might need to install `docsify` and `docsify-cli`
(`$ npm i -g docsify docsify-cli`) before running the documentation server.

[dropbox]: https://developer.android.com/reference/android/os/DropBoxManager.html
