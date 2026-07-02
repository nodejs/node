---
name: trace_processor
description: Managing and querying Perfetto traces using the trace_processor MCP server.
---

# Trace Processor

The `trace_processor` extension launches a long-running FastMCP server wrapping
Perfetto's trace processor capabilities, allowing seamless interaction with
trace files (`.pftrace`, `.perfetto-trace`, `.pb`).

## Writing PerfettoSQL and Schema Analysis

For comprehensive guidance on querying traces, utilizing standard library
modules, and schema structures, refer directly to the native Perfetto AI skills
inside `third_party/perfetto/ai/skills`:

- **Querying Traces**: See the
  [perfetto_infra_querying_traces](../../../../../third_party/perfetto/ai/skills/perfetto_infra_querying_traces/SKILL.md)
  skill.
- **Common SQL Tables**: Explore standard table definitions such as `slice`,
  `thread`, `process`, `sched_slice`, and `track`.
- **Perfetto Standard Library**: Utilize standard library modules like
  `slices.with_context` by starting queries with `INCLUDE PERFETTO MODULE`.

## Official Perfetto References

- Trace Processor reference:
  <https://perfetto.dev/docs/analysis/trace-processor>
- Generated SQL Tables: <https://perfetto.dev/docs/analysis/sql-tables>
- Standard Library Documentation:
  <https://perfetto.dev/docs/analysis/stdlib-docs>
