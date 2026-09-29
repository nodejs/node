---
name: execution-constraints
trigger: always_on
---

# Execution Constraints & Loop Prevention

To ensure efficient operation and prevent resource waste:

01. **No Tool Looping**: Ensure tool calls are not executed with the same
    arguments repeatedly. If a tool call fails or returns the same content,
    change your strategy (e.g., use `code_search` or `grep_search` instead of
    reading blindly).

02. **Surgical File Reading**: Target file reading to specific sections.
    Pinpoint usage using search tools first.

03. **No Browser Usage**: Rely on specialized text-based tools. Ask the user for
    content or guidance if a resource is inaccessible through available tools
    (like Buganizer MCP).

04. **Mandatory Orchestration**: For any complex task or project in V8, the
    agent MUST act as an Orchestrator and use the multi-layered skill framework
    defined in `agents/skills/`. Break down tasks and delegate to subagents to
    maximize parallelism. Avoid sequential execution of independent tasks.

05. **Avoid Interactive Pagers**: Always use `--no-pager` or ensure `PAGER=cat`
    is set when running commands that might produce long output (e.g.,
    `git branch`, `git log`), to avoid getting stuck in a pager waiting for user
    input.

06. **Resuming Tasks**: When resuming a task, verify the current state (e.g.,
    active branch, file contents) rather than assuming previous state persists.

07. **No Environment-Specific Data**: Do not commit environment-specific data.

08. **Base New Branches on Clean Upstream**: Always base new branches on a clean
    upstream (e.g., `origin/main`). Avoid creating spurious CLs or polluting
    existing CLs with unrelated changes.

09. **Verify CL Contents Before Upload**: Before running `git cl upload`, ALWAYS
    verify that the diff relative to upstream contains ONLY the intended
    changes. Use `git diff --name-only origin/main..HEAD` to check the list of
    files being uploaded. If unrelated files are present, remove them before
    uploading.

10. **Respect Existing CL Descriptions**: **NEVER** run
    `git cl upload --commit-description=+` or `git cl desc -n` with a generic
    file to upload subsequent patchsets unless you have explicitly verified that
    the user has not made Web UI edits. Omit `--commit-description` on
    subsequent uploads to preserve human-authored online edits.

11. **Explicit Absolute Paths**: When presenting or linking to files/directories
    that are outside the current workspace directory (such as the App Data
    Directory, the brain directory, scratch files, or conversation logs), ALWAYS
    print their full absolute path.
