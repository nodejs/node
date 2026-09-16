# `trace_events` Tests

When the `node` binary is built with configure flag `--with-perfetto`,
the tests in this folder depends on `tools/perfetto/trace_processor_shell`,
which is downloaded with `tools/perfetto/get_trace_processor` via
`make trace-processor`, to convert Perfetto binary trace files to
JSON format.

Refer to <https://perfetto.dev/docs/reference/trace-processor-cli>
for help of the `trace_processor_shell` CLI.
