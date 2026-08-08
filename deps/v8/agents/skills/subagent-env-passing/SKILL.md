---
name: subagent-env-passing
description: Workflow for passing environment variables (like PATH) from the Orchestrator to subagents. Use when spawning subagents that need specific environment variables. Do not use for standard subagent execution.
---

# Subagent Environment Passing Skill

This skill documents how to ensure subagents have access to the correct
environment variables (such as `PATH`) when working in isolated environments
like worktrees.

## Workflow

1. **Retrieve Environment**: The Orchestrator should read the necessary
   environment variables from its own context if they are not already known. For
   example, run `echo $PATH` to get the current PATH.
2. **Pass to Subagent**: When invoking a subagent, include the environment
   variable values in the `Prompt` provided to the subagent.
3. **Instruct Subagent**: Explicitly instruct the subagent to set these
   variables in its environment before running tools that depend on them.

## Example Prompt Snippet

```
Please use the following PATH and build settings in your environment:
export PATH=<path_to_depot_tools>:$PATH
export SISO_PROJECT=rbe-chrome-untrusted

```

Note: If building or running `gm.py`, also ensure that `use_remoteexec = true`
is set in your build config/`args.gn`. You can find the correct `SISO_PROJECT`
in `build/config/siso/.sisoenv`.
