---
name: v8_log
description: "Understanding v8.log files."
---

# V8 Log Viewer CLI

A standalone command-line tool for analyzing `v8.log` files.
It allows you to inspect internal state of the V8 JavaScript engine.

## Requirements

This tool requires the V8 shell (`d8`) to be installed and accessible.
If no local d8 build is available it will try to call back to jsvu's d8 in
`~/.jsvu/bin/v8`. You can override this by setting the `V8_PATH` environment variable.

## Available Commands

Use the following commands to inspect different aspects of the log.
Each command focuses on a specific type of event or structure in the V8
execution profile:

- **`stats`**
  Prints aggregated statistics of all events found in the log.
  Use this to get a quick high-level overview of the log content
  (e.g., total number of scripts, functions, ICs, and deopts).
- **`ic`**
  Lists detailed information about Inline Cache (IC) events.
  ICs are used by V8 to optimize property accesses based on type feedback.
  Use this to find polymorphic or megamorphic IC misses that might be causing
  performance bottlenecks.
- **`script`**
  Lists all scripts loaded by V8. Use this to find script
  IDs for specific source files or to inspect the size of loaded assets.
- **`code`**
  Lists all code objects created by V8 (e.g., full code, bytecode, stub
  handlers). Use this to inspect the generated code entries and their size for
  specific functions or builtins.
- **`function`**
  Lists all compiled functions tracked by V8 and their variants.
  A function can have multiple compilation variants (e.g., unoptimized bytecode
  and optimized machine code). Use this to see how many times a function was
  compiled and at what optimization tiers.
- **`deopt`**
  Lists all deoptimizations that occurred. Deoptimizations happen when V8's
  optimistic assumptions are violated, forcing it to fall back to less
  optimized code. Use this to find performance cliffs caused by deopts
  and understand why they happened (e.g., insufficient type feedback).

## Usage & Help

To see the full list of common options and specific filter options for any
subcommand, rely on the tool's built-in help system:

```bash
./tools/v8-logviewer --help
```

To see specific help, detailed descriptions, and command-specific filter
options for any subcommand, run:

```bash
./tools/v8-logviewer <command> --help
```

## Logging Flags

To generate a `v8.log` with specific information, you must pass diagnostic flags to `d8` when running your script.

Here are the most common flags to enable specific log events:

- **`--prof`**
  Enables execution profiling (ticks). This is required for `stats` and general profiling analysis.
- **`--log-ic`** (slow)
  Logs Inline Cache (IC) events. Required for the `ic` command.
- **`--log-deopt`**
  Logs deoptimizations. Required for the `deopt` command.
- **`--log-maps`** (slow)
  Logs map creation and modification events.
- **`--log-code`**
  Logs code events to the log file without profiling. Useful for the `code` command.
- **`--log-code-disassemble`**
  Logs all disassembled code to the log file.
- **`--log-source-code`**
  Logs source code information.
- **`--log-all`**
  Enables all logging (very verbose!). Use with caution.

**Example:**
```bash
out/release/d8 --log-all my-script.js
```

This will generate a large v8.log file. It is recommended to use a more targeted set of flags when possible.

## Examples

List the 10 largest script summaries to see which ones are interesting:

```bash
./tools/v8-logviewer script v8.log --sort=size --limit=10
```

List details for a specific script id:

```bash
./tools/v8-logviewer script v8.log --script-id=170 --details
```

List all functions details for a specific script:

```bash
./tools/v8-logviewer function v8.log --script=example.js
```


List the last 5 deoptimizations:

```bash
./tools/v8-logviewer deopt v8.log --limit=-5
```

Find all optimized code objects for a specific function:

```bash
./tools/v8-logviewer code v8.log --function=myFunction --code-kind=Opt
```

Count how many IC events happened in a specific time window:

```bash
./tools/v8-logviewer ic v8.log --time=500000..600000 --count
```
