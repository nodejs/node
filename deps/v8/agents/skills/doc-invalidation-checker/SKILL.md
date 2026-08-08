---
name: doc-invalidation-checker
description: Analyzes recent commits and uses a smart AI subagent to check if they invalidate any documentation in docs/.
---

# Documentation Invalidation Checker

Use this skill when you need to determine if recent commits invalidate information within the documentation in the `docs/` directory.

## Steps to Perform Analysis

1. **Retrieve Recent Commits**:
   Execute the `get_recent_commits.py` script to fetch the recent commits with their diffs:
   ```bash
   agents/scripts/get_recent_commits.py 5
   ```

2. **Identify Relevant Documentation**:
   Look at the modified files and descriptions in the commit diff. Identify which files in `docs/` discuss the modified modules or concepts.

3. **Invoke the Subagent for AI Analysis**:
   Use the `invoke_subagent` tool to call a subagent with the `pro` model tier. 
   Provide the subagent with:
   - The commit message and diff.
   - The content of the relevant file(s) from `docs/`.

4. **Subagent Analysis Prompt**:
   The subagent should receive a prompt like:
   > Compare this commit diff with the documentation file `[doc_path]`. Does this commit make any statement, step, or configuration in the documentation obsolete, incorrect, or deprecated? If yes, specify exactly what section is invalidated and provide recommended text edits.

5. **Document and Present**:
   Collate the invalidation findings into a walkthrough report for update.

## Tools Used
- **[get_recent_commits.py](../../scripts/get_recent_commits.py)**: Retrieves recent commits and their diffs.
