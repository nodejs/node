# Perfetto <-> Ftrace interoperability

*** note
**This doc is WIP**, stay tuned.
<!-- TODO(primiano): write ftrace doc. -->
***

This doc should:
- Describe the ftrace trace_pipe_raw -> protobuf translation.
- Describe how we deal with kernel ABI (in)stability and ftrace fields changing
  over kernel versions (we process `event/**/format files on-device`).
- Describe how to generate ftrace protos (`tools/pull_ftrace_format_files.py`,
  `tools/udate_protos.py`)
- Describe how session multiplexing works.

Code lives in [/src/traced/probes/ftrace](/src/traced/probes/ftrace/).
