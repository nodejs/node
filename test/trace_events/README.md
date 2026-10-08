# `trace_events` Tests

When the `node` binary is built with configure flag `--with-perfetto`,
the tests in this folder depends on `tools/perfetto/trace_processor_shell`,
which is downloaded with `tools/perfetto/get_trace_processor` via
`make tools/perfetto/trace_processor_shell`, to convert Perfetto binary trace
files to JSON format. Set `TRACE_PROCESSOR_SHELL_PATH` to use an existing build
of the tool instead of downloading a release.

Refer to <https://perfetto.dev/docs/reference/trace-processor-cli>
for help of the `trace_processor_shell` CLI.
