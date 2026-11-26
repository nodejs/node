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

## 1. Authentication (OAuth Token Helper & `cookies.txt` Fallback)

`fetch_testcase.py` supports two authentication methods in order:

1. **OAuth Bearer Token (`CLUSTERFUZZ_OAUTH_TOKEN_CMD` / `--oauth-token-cmd`)**:
   When `CLUSTERFUZZ_OAUTH_TOKEN_CMD` is set in the environment (or
   `--oauth-token-cmd` is passed), `fetch_testcase.py` executes that command to
   obtain an OAuth access token (accepting either a plain token or JSON
   `{"token": "..."}` / `{"access_token": "..."}` on stdout) and calls
   ClusterFuzz's OAuth-enabled endpoints (`/testcase-detail/refresh` and
   `/testcase-detail/download-testcase?id=<ID>`), stripping the `Authorization`
   header when following signed GCS redirects. Pass `--no-oauth` to disable
   OAuth.
2. **Browser `cookies.txt` Fallback**: If no OAuth token command is configured
   or OAuth returns HTTP 401/403 (e.g., a restricted bug only lists a different
   account), `fetch_testcase.py` falls back to a Netscape-formatted cookies file
   at `~/.config/clusterfuzz/cookies.txt` (or an explicit
   `--cookies-file <path>`, which disables OAuth).

**Manual `cookies.txt` Setup (When OAuth Is Not Configured or Denied)**: The
agent CANNOT generate or export `cookies.txt` itself. If OAuth is unavailable or
fails and `cookies.txt` is missing or expired, ask the user to perform these
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
handle OAuth, cookie fallback, signed GCS redirects, and metadata:

```bash
python3 agents/skills/clusterfuzz/scripts/fetch_testcase.py <target> [options]
```

- **Download Test Case**:

  ```bash
  python3 agents/skills/clusterfuzz/scripts/fetch_testcase.py <TESTCASE_ID_OR_URL> [-o <output_path>]
  ```

  Uses `CLUSTERFUZZ_OAUTH_TOKEN_CMD` (or `--oauth-token-cmd`) when configured
  and falls back to `~/.config/clusterfuzz/cookies.txt`. If `-o` is omitted,
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

| Issue                              | Cause                                                                              | Fix                                                                                                                                                                                           |
| :--------------------------------- | :--------------------------------------------------------------------------------- | :-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **HTTP 401 (`UnauthorizedError`)** | Expired OAuth credentials, or OAuth client/account not allowlisted on ClusterFuzz. | Refresh your OAuth credentials, ensure the OAuth client/email is allowlisted in ClusterFuzz's auth config, or fall back to `--cookies-file` / `cookies.txt`.                                  |
| **HTTP 403 (`AccessDeniedError`)** | Authenticated account lacks access to this restricted bug/testcase.                | Try an account listed on the bug (e.g., via `cookies.txt`), check Buganizer attachments, or ask the user for testcase flags and content.                                                      |
| **Response is HTML (~900 KB)**     | Missing or expired `cookies.txt` on `clusterfuzz.com` (and OAuth unconfigured).    | Configure `CLUSTERFUZZ_OAUTH_TOKEN_CMD`, or ask the user to re-export `cookies.txt` via [Get cookies.txt LOCALLY](https://chromewebstore.google.com/detail/cclelndahbckbenkjhflpdbgdldlbecc). |
| **Inaccessible / Redacted**        | Restricted permissions.                                                            | Check Buganizer comments for attached POC, or ask user for testcase flags and content.                                                                                                        |

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
