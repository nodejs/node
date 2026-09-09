# USDT probe headers

This directory contains tooling for the `diagnostics_channel` USDT
(User-Level Statically Defined Tracing) probes.

## Why the Linux probe header is committed

The probe definitions live in [`src/node_provider.d`][d]. On Linux the
probe header is *not* generated at build time. It is generated with the
SystemTap `dtrace` wrapper and committed as
[`src/node_provider_linux.h`][h] instead, so that:

* building Node.js with USDT support on Linux requires only
  `<sys/sdt.h>` (the `systemtap-sdt-dev` package on Debian/Ubuntu,
  `systemtap-sdt-devel` on Fedora/RHEL) — no `dtrace` tool, and
* the same probe header is used by every Linux build, so a missing or
  misbehaving `dtrace` tool can never silently change the build.

Linux is the only platform that works this way because the SystemTap
`dtrace -h` output is portable across kernels (it only depends on
`<sys/sdt.h>`), while native DTrace implementations (macOS, FreeBSD,
illumos) produce platform-specific headers and, except on macOS, require
extra `dtrace -G` link-time processing that is not implemented. On macOS
the header is generated at build time when configuring with
`--with-dtrace`.

## Regenerating

After changing `src/node_provider.d`:

```console
$ python3 tools/usdt/generate_headers.py
wrote /path/to/node/src/node_provider_linux.h
```

The SystemTap `dtrace` wrapper must be in `PATH` (it is installed with
`systemtap-sdt-dev`/`systemtap-sdt-devel`; override the binary with
`--dtrace` or the `DTRACE` environment variable). Native DTrace
implementations are rejected because their output is not the committed
format.

Commit the result together with the `src/node_provider.d` change.

## Drift check

CI (`test-usdt` job in `.github/workflows/test-linux.yml`) verifies that
the committed header matches `src/node_provider.d`:

```console
$ python3 tools/usdt/generate_headers.py --check
/path/to/node/src/node_provider_linux.h is up to date
```

[d]: ../../src/node_provider.d
[h]: ../../src/node_provider_linux.h
