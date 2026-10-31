#!/usr/bin/python3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""End-to-end automated coverage report generator.

Pulls the tip-of-tree coverage, resolves affected blocks, fetches
individual file coverage pages, extracts coverage data,
and compiles a beautiful markdown summary report.
"""

import argparse
import concurrent.futures
import json
import os
import re
import subprocess
import sys
import tempfile
import requests

# Add scripts directory to path to reuse existing modules
sys.path.append(os.path.dirname(os.path.abspath(__file__)))
from get_v8_revision import retrieve_v8_version
from find_affected_blocks import HashResolver, FileBlocks
from extract_coverage import get_coverage

FUZZILLI_DASHBOARD_URL = ("https://analysis.chromium.org/coverage/p/chromium"
                          "?platform=fuzzilli-x64&test_suite_type=any&path=//"
                          "&host=chromium.googlesource.com&project=chromium/src"
                          "&ref=refs/heads/main&modifier_id=0")

BUILDBUCKET_SEARCH_URL = (
    "https://cr-buildbucket.appspot.com/prpc/buildbucket.v2.Builds/SearchBuilds"
)
V8_CI_COVERAGE_GCS_PREFIX = (
    "https://storage.googleapis.com/chromium-v8/coverage/ci/linux64_rel")


def run_command(args, capture=True):
  res = subprocess.run(args, capture_output=capture, text=True)
  if res.returncode != 0:
    raise Exception(f"Command failed: {' '.join(args)}\nError: {res.stderr}")
  return res.stdout.strip() if capture else ""


def ensure_revision_in_git(v8_revision):
  """Ensures v8_revision exists in the local git object database without mutating HEAD."""
  check = subprocess.run(["git", "cat-file", "-e", f"{v8_revision}^{{commit}}"],
                         capture_output=True,
                         text=True)
  if check.returncode == 0:
    return True
  print(f"Fetching V8 revision {v8_revision[:7]} into git object store...")
  fetch = subprocess.run(
      ["git", "fetch", "--no-write-fetch-head", "origin", v8_revision],
      capture_output=True,
      text=True)
  if fetch.returncode == 0:
    return True
  return False


def fetch_url(url):
  """Fetches HTML content from a URL."""
  response = requests.get(url)
  response.raise_for_status()
  return response.text


def get_chromium_revision():
  """Fetches the fuzzilli dashboard and extracts the Chromium commit hash."""
  print("Fetching Fuzzilli dashboard to determine tip-of-tree revision...")
  html = fetch_url(FUZZILLI_DASHBOARD_URL)

  # Try to extract full revision from the V8 link first (most precise)
  rev_match = re.search(r'revision=([0-9a-f]{40})', html)
  if rev_match:
    return rev_match.group(1)

  # Fallback to title tag 7-char hash
  title_match = re.search(
      r'<title>Coverage Summary for chromium/src - ([0-9a-f]{7}) - //', html)
  if title_match:
    return title_match.group(1)

  raise Exception(
      "Could not find Chromium commit hash in Fuzzilli dashboard HTML.")


def get_ci_test_coverage_revision():
  """Fetches the latest V8 Linux64 - coverage build revision and report URL."""
  print(
      "Querying 'V8 Linux64 - coverage' builder for the latest test coverage report..."
  )
  headers = {"Content-Type": "application/json", "Accept": "application/json"}
  payload = {
      "predicate": {
          "builder": {
              "project": "v8",
              "bucket": "ci",
              "builder": "V8 Linux64 - coverage"
          },
      },
      "pageSize":
          15,
      "fields": ("builds.*.id,builds.*.number,builds.*.status,"
                 "builds.*.output.properties,builds.*.steps"),
  }
  response = requests.post(
      BUILDBUCKET_SEARCH_URL, headers=headers, json=payload)
  response.raise_for_status()
  text = response.text
  if text.startswith(")]}'"):
    text = text[4:]
  data = json.loads(text)

  candidates = []
  for build in data.get("builds", []):
    rev = build.get("output", {}).get("properties", {}).get("got_revision")
    report_url = None
    for step in build.get("steps", []):
      if step.get("name") == "Code coverage":
        summary = step.get("summaryMarkdown", "")
        match = re.search(r'\((https://[^\s)]+/index\.html)\)', summary)
        if match:
          report_url = match.group(1)[:-len("/index.html")]
          if not rev:
            rev_match = re.search(r'/([0-9a-f]{40})$', report_url)
            if rev_match:
              rev = rev_match.group(1)
        break
    if rev and report_url:
      candidates.append((build.get("number"), rev, report_url))
    elif rev and build.get("status") == "SUCCESS":
      candidates.append(
          (build.get("number"), rev, f"{V8_CI_COVERAGE_GCS_PREFIX}/{rev}"))

  if not candidates:
    raise Exception(
        "Could not find any finished 'V8 Linux64 - coverage' build with a coverage report."
    )

  for build_num, rev, report_url in candidates:
    if ensure_revision_in_git(rev):
      return build_num, rev, report_url

  # Fallback to the latest candidate if git fetch was unavailable
  return candidates[0]


def fetch_file_coverage(file_path,
                        temp_dir,
                        source="fuzzilli",
                        chromium_revision=None,
                        ci_report_base_url=None):
  """Fetches the coverage HTML for a specific V8 file and saves it locally."""
  if source == "fuzzilli":
    coverage_url = (
        f"https://analysis.chromium.org/coverage/p/chromium/file"
        f"?host=chromium.googlesource.com&project=chromium/src&ref=refs/heads/main"
        f"&revision={chromium_revision}&path=//v8/{file_path}"
        f"&platform=fuzzilli-x64&test_suite_type=any&modifier_id=0")
  else:
    coverage_url = (
        f"{ci_report_base_url}/coverage/b/s/w/ir/cache/builder/v8/{file_path}.html"
    )

  try:
    print(f"Fetching {source} coverage for {file_path}...")
    response = requests.get(coverage_url)
    if response.status_code == 404 and source != "fuzzilli":
      # llvm-cov omits HTML files for headers/files with 0 instrumented lines on Linux64
      return file_path, "NOT_INSTRUMENTED"
    response.raise_for_status()
    html_content = response.text

    temp_file_path = os.path.join(
        temp_dir, f"{source}_" + file_path.replace("/", "_") + ".html")
    with open(temp_file_path, "w") as f:
      f.write(html_content)
    return file_path, temp_file_path
  except Exception as e:
    print(
        f"Warning: Failed to fetch {source} coverage for {file_path}: {e}",
        file=sys.stderr)
    return file_path, None


NON_X64_OR_TEST_PATH_RE = re.compile(
    r"^(test|tools)/|"
    r"/(arm|arm64|ia32|loong64|mips64|ppc|ppc64|riscv|riscv32|riscv64|s390|s390x)/|"
    r"-(arm|arm64|ia32|loong64|mips64|ppc|ppc64|riscv|riscv32|riscv64|s390|s390x)(-|\.)"
)

PREPROC_IF_RE = re.compile(r"^#\s*(if|ifdef|ifndef)\b")
PREPROC_ELIF_RE = re.compile(r"^#\s*elif\b")
PREPROC_ELSE_RE = re.compile(r"^#\s*else\b")
PREPROC_ENDIF_RE = re.compile(r"^#\s*endif\b")

FUNC_HEADER_RE = re.compile(
    r"^\s*(?:[\w:<>\*~&]+\s+)+([A-Za-z_~][\w:<>]*)\s*\([^;]*$")
BENIGN_LINE_RE = re.compile(
    r"^\s*(UNREACHABLE\s*\(\)\s*;?|DCHECK[_A-Z]*\s*\(.*\)\s*;?|"
    r"CHECK[_A-Z]*\s*\(.*\)\s*;?|default\s*:|break\s*;|"
    r"return\s+Next::Reduce.*|\}\s*(else\b.*)?|//.*)?\s*$")


def is_non_x64_or_test_file(file_path):
  """Returns True if file_path is a test/tool or non-x64 backend file."""
  return bool(NON_X64_OR_TEST_PATH_RE.search(file_path))


def get_file_lines_at_revision(file_path, v8_revision):
  """Returns 1-indexed list of lines (index 0 unused) at v8_revision."""
  try:
    content = run_command(["git", "show", f"{v8_revision}:{file_path}"])
    return [""] + content.splitlines()
  except Exception:
    if os.path.isfile(file_path):
      with open(file_path, "r", errors="replace") as f:
        return [""] + f.read().splitlines()
    return []


def _full_preproc_directive(lines, idx):
  """Joins backslash-continued preprocessor directive lines starting at idx."""
  parts = []
  cur = idx
  while cur < len(lines):
    s = lines[cur].strip()
    if s.endswith("\\"):
      parts.append(s[:-1].strip())
      cur += 1
    else:
      parts.append(s)
      break
  return " ".join(parts)


def find_enclosing_context(lines, start_line):
  """Finds the nearest enclosing function signature or #if guard above start_line."""
  enclosing_func = None
  enclosing_ifdef = None
  ifdef_depth = 0
  in_else_branch = False
  else_line_idx = None
  active_elif = None

  for idx in range(
      min(start_line,
          len(lines) - 1), max(0, start_line - 120), -1):
    line = lines[idx]
    stripped = line.strip()
    if enclosing_ifdef is None:
      if PREPROC_ENDIF_RE.match(stripped):
        ifdef_depth += 1
      elif PREPROC_ELSE_RE.match(stripped):
        if ifdef_depth == 0 and not in_else_branch and active_elif is None:
          in_else_branch = True
          else_line_idx = idx
      elif PREPROC_ELIF_RE.match(stripped):
        if ifdef_depth == 0 and not in_else_branch and active_elif is None:
          active_elif = f"L{idx}: {_full_preproc_directive(lines, idx)}"
      elif PREPROC_IF_RE.match(stripped):
        if ifdef_depth > 0:
          ifdef_depth -= 1
        else:
          full_dir = _full_preproc_directive(lines, idx)
          if active_elif is not None:
            enclosing_ifdef = active_elif
          elif in_else_branch:
            enclosing_ifdef = (
                f"L{else_line_idx}: #else (negating L{idx}: {full_dir})")
          else:
            enclosing_ifdef = f"L{idx}: {full_dir}"
      elif ifdef_depth == 0 and "if constexpr" in stripped:
        enclosing_ifdef = f"L{idx}: {stripped}"

    if enclosing_func is None and FUNC_HEADER_RE.match(line):
      enclosing_func = f"L{idx}: {stripped}"
      if enclosing_ifdef is not None:
        break
  return enclosing_func, enclosing_ifdef


def _has_positive_token(expr, tokens):
  """Returns True if any token in tokens appears in expr without a preceding '!'."""
  for tok in tokens:
    for m in re.finditer(rf"\b{tok}\b", expr):
      prefix = expr[:m.start()]
      if not re.search(r"!\s*(?:defined\s*\(\s*)?$", prefix):
        return True
  return False


def _has_negated_token(expr, tokens):
  """Returns True if any token in tokens is directly negated by '!' in expr."""
  for tok in tokens:
    if re.search(rf"!\s*(?:defined\s*\(\s*)?\b{tok}\b", expr):
      return True
  return False


def _is_non_x64_ifdef(enclosing_ifdef):
  if not enclosing_ifdef:
    return False
  non_x64_tokens = (
      "V8_TARGET_ARCH_32_BIT",
      "V8_TARGET_ARCH_IA32",
      "V8_TARGET_ARCH_ARM",
      "V8_TARGET_ARCH_ARM64",
      "V8_TARGET_ARCH_PPC",
      "V8_TARGET_ARCH_PPC64",
      "V8_TARGET_ARCH_S390",
      "V8_TARGET_ARCH_S390X",
      "V8_TARGET_ARCH_MIPS64",
      "V8_TARGET_ARCH_LOONG64",
      "V8_TARGET_ARCH_RISCV",
      "V8_TARGET_ARCH_RISCV32",
      "V8_TARGET_ARCH_RISCV64",
  )
  x64_tokens = (
      "V8_TARGET_ARCH_64_BIT",
      "V8_TARGET_ARCH_X64",
  )
  if ": #else (negating " in enclosing_ifdef:
    negated_part = enclosing_ifdef.split(": #else (negating ", 1)[1]
    if "#ifndef " in negated_part:
      return (_has_positive_token(negated_part, non_x64_tokens) and
              not _has_positive_token(negated_part, x64_tokens))
    if _has_positive_token(negated_part, x64_tokens):
      return True
    if (_has_negated_token(negated_part, non_x64_tokens) and
        not _has_negated_token(negated_part, x64_tokens)):
      return True
    return False
  if "#ifndef " in enclosing_ifdef:
    return _has_positive_token(enclosing_ifdef, x64_tokens)
  if _has_positive_token(enclosing_ifdef, x64_tokens):
    return False
  if _has_negated_token(enclosing_ifdef, x64_tokens):
    return True
  return _has_positive_token(enclosing_ifdef, non_x64_tokens)


def classify_uncovered_block(file_path,
                             lines,
                             start_line,
                             end_line,
                             enclosing_func,
                             enclosing_ifdef,
                             source="fuzzilli"):
  """Heuristic auto-triage hint for an uncovered block."""
  block_lines = [
      lines[i] for i in range(start_line, min(end_line + 1, len(lines)))
  ]
  joined = "\n".join(block_lines)
  if block_lines and all(BENIGN_LINE_RE.match(l) for l in block_lines):
    return "Benign — UNREACHABLE / DCHECK / control-flow boilerplate"
  if "V8_TARGET_ARCH_64_BIT" in joined and "UNREACHABLE()" in joined:
    return "Benign — 32-bit only (UNREACHABLE on 64-bit)"
  if "ShouldSkipOptimizationStep" in joined:
    return "Benign — Optimization bisection guard"
  if (enclosing_func and "PrintOptions" in enclosing_func) or ("PrintOptions"
                                                               in joined):
    return "Benign — IR printing / diagnostics"
  if (enclosing_func and "inputs_rep" in enclosing_func) or ("inputs_rep()"
                                                             in joined):
    return "Benign — Turboshaft IR metadata / debug validation"
  if _is_non_x64_ifdef(enclosing_ifdef):
    return "Benign — Non-x64 / 32-bit fallback"
  if source == "fuzzilli" and file_path.endswith("random-module-generation.cc"):
    return "Benign — Wasm libFuzzer random module generator"
  return None


def _generate_single_source_report(commits,
                                   source="fuzzilli",
                                   checkout=False,
                                   show_snippets=True):
  log_lines = []
  chromium_revision = None
  ci_report_base_url = None
  ci_build_num = None

  if source == "fuzzilli":
    chromium_revision = get_chromium_revision()
    v8_revision = retrieve_v8_version(chromium_revision)
    ensure_revision_in_git(v8_revision)
    log_lines.append(
        f"Fuzzilli Tip of Tree: Chromium={chromium_revision[:7]} -> V8={v8_revision[:7]}"
    )
  else:
    ci_build_num, v8_revision, ci_report_base_url = (
        get_ci_test_coverage_revision())
    log_lines.append(
        f"V8 Linux64 - coverage (Build #{ci_build_num}): V8={v8_revision[:7]} ({ci_report_base_url}/index.html)"
    )

  # Handle git checkout if requested
  current_revision = run_command(["git", "rev-parse", "HEAD"])
  if checkout and current_revision != v8_revision:
    log_lines.append(f"Checking out V8 revision {v8_revision[:7]}...")
    run_command(["git", "fetch"])
    run_command(["git", "checkout", v8_revision])

  # Find affected files and blocks (skipping non-x64/test files before git blame)
  log_lines.append(
      f"Analyzing Git diffs and blames against revision {v8_revision[:7]} to identify affected blocks..."
  )
  blocks_analyzer = FileBlocks(
      commits,
      revision=v8_revision,
      skip_file_predicate=is_non_x64_or_test_file)
  affected_blocks = blocks_analyzer.blocks()
  skipped_files = blocks_analyzer.skipped()

  if not affected_blocks and not skipped_files:
    log_lines.append("No code blocks were affected by the provided references.")
    return "\n".join(log_lines)

  # Group blocks by file
  files_to_blocks = {}
  for block in affected_blocks:
    files_to_blocks.setdefault(block.file, []).append(block)

  # Fetch coverage HTMLs in parallel using ThreadPoolExecutor
  temp_files = {}
  with tempfile.TemporaryDirectory() as temp_dir:
    log_lines.append(
        f"Downloading {source} coverage reports for {len(files_to_blocks)} x64/shared files "
        f"(skipped {len(skipped_files)} non-x64/test files)...")
    with concurrent.futures.ThreadPoolExecutor(max_workers=16) as executor:
      futures = [
          executor.submit(
              fetch_file_coverage,
              file_path,
              temp_dir,
              source=source,
              chromium_revision=chromium_revision,
              ci_report_base_url=ci_report_base_url)
          for file_path in files_to_blocks.keys()
      ]
      for future in concurrent.futures.as_completed(futures):
        file_path, temp_file = future.result()
        if temp_file:
          temp_files[file_path] = temp_file

    # Parse coverage and compile statistics
    if (files_to_blocks and all(
        temp_files.get(fp) == "NOT_INSTRUMENTED" for fp in files_to_blocks)):
      raise Exception(
          f"All {len(files_to_blocks)} requested files returned 404 from "
          f"{ci_report_base_url}. The report URL layout may have changed.")

    covered_lines = 0
    uncovered_lines = 0
    ignored_lines = 0
    per_file_stats = []
    uncovered_blocks_entries = []
    file_coverage_maps = {}

    for file_path in sorted(files_to_blocks.keys()):
      blocks = files_to_blocks[file_path]
      html_file = temp_files.get(file_path)
      if html_file == "NOT_INSTRUMENTED":
        for block in blocks:
          ignored_lines += (block.end - block.begin + 1)
        continue
      if not html_file:
        for block in blocks:
          uncovered_blocks_entries.append(
              (file_path, block.begin, block.end, "No coverage data available"))
        continue

      # Map blocks (including ±3 context lines) to line ranges for extract_coverage
      line_ranges = []
      for block in blocks:
        ctx_begin = max(1, block.begin - 3)
        ctx_end = block.end + 3
        line_ranges.append(f"{ctx_begin}-{ctx_end}")

      with open(html_file, "r") as f:
        coverage_results = get_coverage(f, line_ranges)

      coverage_map = dict(coverage_results)
      file_coverage_maps[file_path] = coverage_map
      file_cov = 0
      file_uncov = 0
      file_ign = 0

      for block in blocks:
        current_uncovered_start = None
        for line_num in range(block.begin, block.end + 1):
          status = coverage_map.get(line_num)
          if status is None or status == "Not found":
            ignored_lines += 1
            file_ign += 1
            if current_uncovered_start is not None:
              uncovered_blocks_entries.append(
                  (file_path, current_uncovered_start, line_num - 1, None))
              current_uncovered_start = None
          elif status == "0":
            uncovered_lines += 1
            file_uncov += 1
            if current_uncovered_start is None:
              current_uncovered_start = line_num
          else:
            covered_lines += 1
            file_cov += 1
            if current_uncovered_start is not None:
              uncovered_blocks_entries.append(
                  (file_path, current_uncovered_start, line_num - 1, None))
              current_uncovered_start = None

        if current_uncovered_start is not None:
          uncovered_blocks_entries.append(
              (file_path, current_uncovered_start, block.end, None))

      file_exec = file_cov + file_uncov
      file_total = file_exec + file_ign
      if file_exec > 0:
        per_file_stats.append(
            (file_path, file_total, file_exec, file_cov, file_uncov, file_ign))

    # Pre-fetch source files at v8_revision in parallel for uncovered blocks
    file_lines_cache = {}
    uncovered_files = sorted(
        {fp for fp, _, _, note in uncovered_blocks_entries if not note})
    if uncovered_files:
      with concurrent.futures.ThreadPoolExecutor(max_workers=16) as executor:
        fut_to_fp = {
            executor.submit(get_file_lines_at_revision, fp, v8_revision): fp
            for fp in uncovered_files
        }
        for fut in concurrent.futures.as_completed(fut_to_fp):
          file_lines_cache[fut_to_fp[fut]] = fut.result()

    # Classify uncovered blocks and compute auto-triaged benign counts per file
    file_benign_uncov = {}
    classified_entries = []
    for file_path, start_line, end_line, note in uncovered_blocks_entries:
      enclosing_func = None
      enclosing_ifdef = None
      hint = None
      if not note:
        lines = file_lines_cache.get(file_path, [])
        enclosing_func, enclosing_ifdef = find_enclosing_context(
            lines, start_line)
        hint = classify_uncovered_block(
            file_path,
            lines,
            start_line,
            end_line,
            enclosing_func,
            enclosing_ifdef,
            source=source)
        if hint and hint.startswith("Benign"):
          file_benign_uncov[file_path] = (
              file_benign_uncov.get(file_path, 0) + (end_line - start_line + 1))
      classified_entries.append((file_path, start_line, end_line, note,
                                 enclosing_func, enclosing_ifdef, hint))

    total_benign_uncov = sum(file_benign_uncov.values())
    total_needs_triage = uncovered_lines - total_benign_uncov

    # Build markdown report
    out = list(log_lines)
    report_title = ("FUZZILLI LINE COVERAGE SUMMARY REPORT"
                    if source == "fuzzilli" else
                    "V8 LINUX64 TEST COVERAGE SUMMARY REPORT")
    out.append("\n" + "=" * 80)
    out.append(f"                  {report_title}")
    out.append("=" * 80)

    total_lines = covered_lines + uncovered_lines + ignored_lines
    executable_lines = covered_lines + uncovered_lines
    covered_pct = (covered_lines / executable_lines *
                   100) if executable_lines > 0 else 0
    uncovered_pct = (uncovered_lines / executable_lines *
                     100) if executable_lines > 0 else 0
    effective_exec = executable_lines - total_benign_uncov
    effective_pct = (covered_lines / effective_exec *
                     100) if effective_exec > 0 else 100.0

    out.append("\n### Summary Metrics")
    if source == "fuzzilli":
      out.append(
          f"* **Coverage source:** Fuzzilli (`Chromium {chromium_revision[:7]}` / `V8 {v8_revision[:7]}`)"
      )
    else:
      out.append(
          f"* **Coverage source:** V8 Linux64 - coverage ([Build #{ci_build_num}]({ci_report_base_url}/index.html), `V8 {v8_revision[:7]}`)"
      )
    out.append(f"* **Total lines modified by CLs:** {total_lines}")
    out.append(f"* **Executable lines:** {executable_lines}")
    out.append(
        f"* **Covered lines:** {covered_lines} ({covered_pct:.1f}% raw; {effective_pct:.1f}% excluding auto-triaged benign)"
    )
    out.append(
        f"* **Uncovered lines:** {uncovered_lines} ({uncovered_pct:.1f}%) — "
        f"{total_benign_uncov} auto-triaged benign, **{total_needs_triage} needing triage**"
    )
    out.append(
        f"* **Ignored lines (no coverage info / non-x64 / tests):** {ignored_lines}"
    )

    if per_file_stats:
      out.append("\n### Per-File Coverage Breakdown (Executable on Linux x64)")
      out.append(
          "| File | Total Modified | Executable | Covered | Uncovered (Raw) | Auto-Benign | Needs Triage | Ignored | Raw Cov % | Eff. Cov % |"
      )
      out.append(
          "| :--- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |"
      )
      for (fp, f_tot, f_exec, f_cov, f_uncov, f_ign) in per_file_stats:
        f_benign = file_benign_uncov.get(fp, 0)
        f_triage = f_uncov - f_benign
        pct = (f_cov / f_exec * 100) if f_exec > 0 else 0.0
        eff_denom = f_exec - f_benign
        eff_pct_str = f"{(f_cov / eff_denom * 100):.1f}%" if eff_denom > 0 else "N/A"
        fp_cell = f"**`{fp}`**" if f_triage > 0 else f"`{fp}`"
        out.append(
            f"| {fp_cell} | {f_tot} | {f_exec} | {f_cov} | {f_uncov} | {f_benign} | {f_triage} | {f_ign} | {pct:.1f}% | {eff_pct_str} |"
        )

    out.append("\n### Uncovered Code Blocks")
    if classified_entries:
      for (file_path, start_line, end_line, note, enclosing_func,
           enclosing_ifdef, hint) in classified_entries:
        range_str = (f"{file_path}:{start_line}-{end_line}"
                     if start_line != end_line else f"{file_path}:{start_line}")
        if note:
          out.append(f"- `{range_str}` ({note})")
          continue

        header_suffix = f" — **[{hint}]**" if hint else " — **[Needs Triage]**"
        context_parts = [p for p in (enclosing_func, enclosing_ifdef) if p]
        context_str = (f" *(Context: {'; '.join(context_parts)})*"
                       if context_parts else "")
        out.append(f"- `{range_str}`{header_suffix}{context_str}")
        if show_snippets:
          lines = file_lines_cache.get(file_path, [])
          cov_map = file_coverage_maps.get(file_path, {})
          if lines:
            out.append("  ```cpp")
            ctx_start = max(1, start_line - 2)
            ctx_end = min(len(lines) - 1, end_line + 2)
            for ln in range(ctx_start, ctx_end + 1):
              st = cov_map.get(ln)
              if st == "0":
                tag = "UNCOV  "
              elif st is None or st == "Not found":
                tag = "  ---  "
              else:
                tag = f"cov:{str(st):>3s}"
              marker = ">>" if start_line <= ln <= end_line else "  "
              out.append(f"  {marker}{ln:4d} [{tag}]: {lines[ln]}")
            out.append("  ```")
    else:
      out.append(
          "🎉 **100% Line Coverage!** No uncovered blocks found in modified lines."
      )
    out.append("\n" + "=" * 80)
    return "\n".join(out)


def generate_report(references,
                    features=None,
                    greps=None,
                    source="fuzzilli",
                    checkout=False,
                    show_snippets=True,
                    output_dir=None):
  # Step 1: Resolve references to commit hashes in parallel
  print("Resolving references to commit hashes...")
  hash_resolver = HashResolver()
  hash_resolver.resolve_all(references, features, greps)
  commits = hash_resolver.hashes()
  bugs = sorted(hash_resolver.bugs())
  print(f"Resolved to {len(commits)} commits" +
        (f" across bugs {', '.join(bugs)}" if bugs else "") +
        f": {', '.join(sorted(commits))}")

  if output_dir:
    os.makedirs(output_dir, exist_ok=True)
    with open(os.path.join(output_dir, "commits.txt"), "w") as f:
      f.write("\n".join(sorted(commits)) + "\n")
    with open(os.path.join(output_dir, "bugs.txt"), "w") as f:
      f.write("\n".join(bugs) + "\n")

  norm_source = "tests" if source == "ci" else source
  sources = ["fuzzilli", "tests"] if norm_source == "both" else [norm_source]

  if len(sources) > 1 and not checkout:
    with concurrent.futures.ThreadPoolExecutor(
        max_workers=len(sources)) as executor:
      fut_map = {
          src:
              executor.submit(
                  _generate_single_source_report,
                  commits,
                  source=src,
                  checkout=False,
                  show_snippets=show_snippets) for src in sources
      }
      for src in sources:
        report_text = fut_map[src].result()
        if output_dir:
          out_path = os.path.join(output_dir, f"{src}-coverage.md")
          with open(out_path, "w") as f:
            f.write(report_text + "\n")
        try:
          print(report_text)
        except BrokenPipeError:
          pass
  else:
    for src in sources:
      report_text = _generate_single_source_report(
          commits, source=src, checkout=checkout, show_snippets=show_snippets)
      if output_dir:
        out_path = os.path.join(output_dir, f"{src}-coverage.md")
        with open(out_path, "w") as f:
          f.write(report_text + "\n")
      try:
        print(report_text)
      except BrokenPipeError:
        pass


if __name__ == "__main__":
  parser = argparse.ArgumentParser(
      description="Generate an end-to-end line coverage report.")
  parser.add_argument(
      "reference",
      nargs="*",
      help="Git commit hash, Change-Id, bug ID/link, or feature flag name.")
  parser.add_argument(
      "--feature",
      "-f",
      action="append",
      default=[],
      help="Feature flag name (e.g. wide_arithmetic) to trace across flag files and bugs."
  )
  parser.add_argument(
      "--grep",
      "-g",
      action="append",
      default=[],
      help="Commit message regex/substring (e.g. '\\[wasm-wide-arith\\]') to include."
  )
  parser.add_argument(
      "--source",
      "-s",
      choices=["fuzzilli", "tests", "ci", "both"],
      default="fuzzilli",
      help=(
          "Coverage report source: 'fuzzilli' (Fuzzilli x64 dashboard, default), "
          "'tests' / 'ci' (latest 'V8 Linux64 - coverage' CI build), or 'both'."
      ))
  parser.add_argument(
      "--output-dir",
      "-o",
      help=("Optional directory (e.g. /tmp/v8-coverage) to save commits.txt, "
            "bugs.txt, fuzzilli-coverage.md, and tests-coverage.md."))
  parser.add_argument(
      "--checkout",
      action="store_true",
      help="Automatically checkout the matching V8 revision.")
  parser.add_argument(
      "--no-snippets",
      action="store_true",
      help="Omit source code snippets and auto-triage hints for uncovered blocks."
  )
  args = parser.parse_args()
  if not args.reference and not args.feature and not args.grep:
    parser.error("At least one reference, --feature, or --grep is required.")

  try:
    generate_report(
        args.reference,
        features=args.feature,
        greps=args.grep,
        source=args.source,
        checkout=args.checkout,
        show_snippets=not args.no_snippets,
        output_dir=args.output_dir)
  except Exception as e:
    print(f"\nERROR: {e}", file=sys.stderr)
    sys.exit(1)
