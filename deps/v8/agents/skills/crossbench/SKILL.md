---
name: crossbench
description: "Running benchmarks with Crossbench."
---

# Crossbench

Crossbench is the central chrome runner harness for press benchmarks like
JetStream, Speedometer or Motionmark, as well as for loading live pages.

## Location and Setup

- **Location**: It is often installed in `~/crossbench` or in
  `CHROMIUM_CHECKOUT/src/tools/perf/cb`, but could be anywhere.
  If not found in standard locations, ask the user for the path.
- **Update**: Always ensure you are running the latest version.

## Basic Usage

Navigate to the crossbench directory and use `./cb.py` directly (or via `poetry`
if vpython3 is not available).

```bash
# Run the latest stable JetStream with a specific d8 binary
./cb.py jetstream --browser=/path/to/d8 --env-validation=warn
```

And for chrome:

```bash
./cb.py jetstream --browser=out/x64.release/chrome --env-validation=warn
```

## Filtering

- You can run benchmark workload separate with `--separate` for instance to
  generate individual profiles for each sub test
- Use `--story=XXX` to only run a specific test

## Crossbench MCP

Use `./cb.py mcp` to launch a local MCP to interact with benchmarks.

## Profiling with Probes

- Use `./cb.py describe probes` to list all probes.
- Use `./cb.py describe probe v8.log` to show detailed help for a specific probe.

Important probes for v8 investigation:

- `--probe='v8.log'` to generate a `v8.log` file (see v8-log skill for
  more information on how to analyse the log file)
- `--probe='profiling'` to generate a Linux `perf` trace.
- `--probe=perfetto` to collected perfetto traces

Results:

- Crossbench prints the results directory and provides a summary
  `cb.results.json` file listing all probe results

## Best Practices

- Use `--env-validation=warn` to bypass environment input prompts.
- Use `./cb.py describe` to understand subcommands and probes.
- Prefer creating JSON files instead of HJSON for configurations to minimize quoting errors.
- Validate generated configs with `poetry run cb_validate_hjson -- file.hjson`.

## Known Stories

- Be aware that 'WSL' in JetStream3 refers to the **WebGPU Shading Language**
  workload, not the Windows Subsystem for Linux environment.

## Environment Fallbacks

- Suggest install depot_toools if vpython3 is not available
- Use poetry as backup if vpython3 is not available.
- The JetStream benchmark can be run directly with d8
