---
name: clusterfuzz
description: >-
  Downloads, manages, and prepares test cases and reproducers for ClusterFuzz in V8.
  Use when downloading test cases or fetching reproduction flags/metadata from
  clusterfuzz.com, or preparing crashing POCs and upload packages for ClusterFuzz.
  Don't use for generic git bisect or general bug triage.
---

# Skill: ClusterFuzz Test Case Management and Triage

This skill provides procedures and tooling for interacting with public
ClusterFuzz (`clusterfuzz.com`) test cases in V8, including authenticated
downloads, flag/metadata extraction, reproducer escalation, and upload
packaging. (Note: This skill only works for public `clusterfuzz.com`;
corp-internal `clusterfuzz.corp.google.com` requires `sso_client`).

______________________________________________________________________

## 1. Authentication (Setting Up `cookies.txt`)

To authenticate with `clusterfuzz.com`, the tooling in this skill uses a
Netscape-formatted browser cookies file at `~/.config/clusterfuzz/cookies.txt`.

**Manual User Prerequisite**: The agent CANNOT generate or export `cookies.txt`
itself. If `cookies.txt` is missing or expired, ask the user to perform these
manual steps:

1. Log in to [https://clusterfuzz.com](https://clusterfuzz.com) in Chrome.
2. Export cookies for `clusterfuzz.com` in Netscape format (e.g., using the
   Chrome extension
   [Get cookies.txt LOCALLY](https://chromewebstore.google.com/detail/cclelndahbckbenkjhflpdbgdldlbecc)).
3. Save the file as `~/.config/clusterfuzz/cookies.txt`.

______________________________________________________________________

## 2. Downloading Test Cases & Metadata

### Using `fetch_testcase.py` (Recommended)

Run the bundled [`fetch_testcase.py`](scripts/fetch_testcase.py) script to
handle cookies, redirects, and metadata:

```bash
python3 agents/skills/clusterfuzz/scripts/fetch_testcase.py <target> [options]
```

- **Download Test Case**:

  ```bash
  python3 agents/skills/clusterfuzz/scripts/fetch_testcase.py <TESTCASE_ID_OR_URL> [-o <output_path>]
  ```

  Auto-detects `~/.config/clusterfuzz/cookies.txt`. If `-o` is omitted,
  downloads to a temporary file (`testcase_<id>.js` or `.wasm`).

- **Fetch Metadata & Minimized Flags (`--info`)**:

  ```bash
  python3 agents/skills/clusterfuzz/scripts/fetch_testcase.py <TESTCASE_ID> --info
  ```

  Queries `/testcase-detail/refresh` for minimized `d8` flags, random seed, job
  type, crash revision, regression range, and recommended local `out/`
  invocation.

- **Save Full JSON Report (`--save-metadata`)**:

  ```bash
  python3 agents/skills/clusterfuzz/scripts/fetch_testcase.py <TESTCASE_ID> --save-metadata <metadata.json>
  ```

### Manual Fallback

```bash
curl -b ~/.config/clusterfuzz/cookies.txt -s -L "https://clusterfuzz.com/download?testcase_id=<ID>" -o <output_file>
```

______________________________________________________________________

## 3. Troubleshooting

| Issue                          | Cause                                                  | Fix                                                                                                                                               |
| :----------------------------- | :----------------------------------------------------- | :------------------------------------------------------------------------------------------------------------------------------------------------ |
| **Response is HTML (~900 KB)** | Missing or expired `cookies.txt` on `clusterfuzz.com`. | Ask the user to re-export `cookies.txt` via [Get cookies.txt LOCALLY](https://chromewebstore.google.com/detail/cclelndahbckbenkjhflpdbgdldlbecc). |
| **Inaccessible / Redacted**    | Restricted permissions.                                | Check Buganizer comments for attached POC, or ask user for testcase flags and content.                                                            |

______________________________________________________________________

## 4. Preparing Reproducers for ClusterFuzz

ClusterFuzz automated bisection and regression tracking require a **detectable
crash** (`DCHECK`, segfault, `SIGILL`, or `int3`).

- **Standalone Script**: Must not depend on external files or shell wrappers.
- **Minimal Flags**: Keep only the minimum flags needed to reproduce.
- **Deterministic Crash**: Must reliably crash.
- **WebAssembly Reproducers**: If the testcase is a raw `.wasm` binary, convert
  it to readable JavaScript using `wami`
  (`out/x64.release/wami --mjsunit <file.wasm> > <repro.js>`), following the
  `minimize-reproducer` skill.

### Escalating Silent Memory Corruption:

If a bug causes memory corruption or a logic error without crashing:

1. **Test Sanitized and Debug Builds**: Run against Debug (`x64.debug`), ASan
   (`x64.asan`), or UBSan builds:
   ```bash
   timeout -v 15 out/x64.asan/d8 <flags> poc.js
   ```
2. **Force a Crash**: If no crash occurs naturally, modify the POC to force a
   crash (e.g. overwrite an object Map or pointer with garbage `0x41414141` so
   property access segfaults) while preserving the bug trigger.
3. **Verify on Release**: Confirm the reproducer crashes on Release builds.

______________________________________________________________________

## 5. ClusterFuzz Upload Package

The agent cannot upload directly to the web UI. If a verified crash is not yet
uploaded:

1. **Verify**: Check Buganizer comments to confirm the crash is not already
   uploaded.
2. **Assemble Package for User**:
   - **Reproducer**: Path to minimized crashing script (e.g., `testcase.js` or
     temporary file).
   - **Job Type**: Recommended job (e.g., `linux_asan_d8`, `linux_d8_dbg`,
     `v8_foozzie`).
   - **Flags**: Minimal `d8` execution flags.
   - **Target Revision**: Git commit hash and version from
     `src/utils/version.h`.
   - **Buganizer Issue ID**: Issue ID.
   - **Upload URL**: `https://clusterfuzz.com/upload-testcase`
3. **User Action**: Present the package and advise the user to perform the
   manual upload.
