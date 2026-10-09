#!/usr/bin/env python3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Fetches test cases and metadata from public ClusterFuzz (clusterfuzz.com).

Supports:
- Downloading reproducer scripts (.js or .wasm)
- Fetching issue metadata (minimized d8 flags, job type, regression range, crash stack)
- Automatic cookie jar detection (~/.config/clusterfuzz/cookies.txt)
"""

from __future__ import annotations

import argparse
import http.cookiejar
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
from urllib.error import HTTPError, URLError
import urllib.parse
import urllib.request

CLUSTERFUZZ_DOMAIN = "clusterfuzz.com"
DOWNLOAD_URL_FMT = "https://clusterfuzz.com/download?testcase_id={id}"
REFRESH_URL = "https://clusterfuzz.com/testcase-detail/refresh"

COOKIE_EXTENSION_URL = (
    "https://chromewebstore.google.com/detail/cclelndahbckbenkjhflpdbgdldlbecc")


def print_missing_cookies_help() -> None:
  """Prints instructions for exporting cookies.txt using the Chrome extension."""
  msg = f"""
================================================================================
AUTHENTICATION REQUIRED: Missing or expired cookies for {CLUSTERFUZZ_DOMAIN}
================================================================================
ClusterFuzz returned a login page or required authentication.
To authenticate command-line downloads:
1. Log in to https://clusterfuzz.com in Chrome with your @google.com or @chromium.org account.
2. Export your cookies for clusterfuzz.com in Netscape format.
   Recommended extension: 'Get cookies.txt LOCALLY'
   URL: {COOKIE_EXTENSION_URL}
3. Save the exported cookies file to:
   ~/.config/clusterfuzz/cookies.txt
   (or provide --cookies-file <path>).
================================================================================
"""
  print(msg.strip(), file=sys.stderr)


def find_cookies_file(specified: str | None = None) -> Path | None:
  """Finds cookies.txt from CLI arguments or the default config location."""
  if specified:
    p = Path(specified).expanduser().resolve()
    if p.exists():
      return p
    raise FileNotFoundError(f"Specified cookies file not found: {specified}")

  default_path = Path.home() / ".config" / "clusterfuzz" / "cookies.txt"
  if default_path.exists():
    return default_path
  return None


def parse_target(target: str) -> tuple[str, str]:
  """Parses a target string (ID or URL) into (testcase_id, download_url)."""
  target = target.strip()

  # Check if target is a pure numeric ID
  if re.fullmatch(r"\d+", target):
    return target, DOWNLOAD_URL_FMT.format(id=target)

  parsed = urllib.parse.urlparse(target)
  if parsed.scheme not in ("http", "https") or parsed.hostname not in (
      CLUSTERFUZZ_DOMAIN,
      f"www.{CLUSTERFUZZ_DOMAIN}",
  ):
    raise ValueError(
        f"Invalid ClusterFuzz URL (expected {CLUSTERFUZZ_DOMAIN}): {target}")

  query_params = urllib.parse.parse_qs(parsed.query)

  testcase_id = None
  for param in ("testcase_id", "id", "key"):
    if param in query_params:
      candidate = query_params[param][0]
      if not re.fullmatch(r"\d+", candidate):
        raise ValueError(
            f"Invalid non-numeric testcase ID in URL query: {candidate!r}")
      testcase_id = candidate
      break

  if not testcase_id:
    m = re.search(r"/(?:testcase-detail|testcase|download)/(\d+)(?:/|$)",
                  parsed.path)
    if m:
      testcase_id = m.group(1)
    else:
      m2 = re.search(r"/(\d+)/?$", parsed.path)
      if m2:
        testcase_id = m2.group(1)

  if not testcase_id or not re.fullmatch(r"\d+", testcase_id):
    raise ValueError(
        f"Could not extract a valid numeric testcase ID from target: {target}")

  return testcase_id, DOWNLOAD_URL_FMT.format(id=testcase_id)


def is_html_login_response(content: bytes) -> bool:
  """Checks if content is an HTML login shell or error page instead of a testcase."""
  header_sample = content[:4096].lower()
  if b"<!doctype html" in header_sample or b"<html" in header_sample:
    return True
  content_lower = content.lower()
  if b"/session-login" in content_lower or b"authresult.user.getidtoken" in content_lower:
    return True
  return False


def download_curl(url: str, cookies_file: Path | None) -> bytes:
  """Downloads via curl, leveraging Netscape cookie jar if available."""
  cmd = ["curl", "-s", "-L"]
  if cookies_file:
    cmd.extend(["-b", str(cookies_file)])
  cmd.append(url)

  res = subprocess.run(cmd, capture_output=True, check=False)
  if res.returncode != 0:
    err_msg = res.stderr.decode("utf-8", errors="replace").strip()
    raise RuntimeError(f"curl failed (exit {res.returncode}): {err_msg}")
  return res.stdout


def download_urllib(url: str, cookies_file: Path | None) -> bytes:
  """Downloads via urllib, using MozillaCookieJar if cookies file is available."""
  handlers: list[urllib.request.BaseHandler] = []
  if cookies_file:
    try:
      cj = http.cookiejar.MozillaCookieJar(str(cookies_file))
      cj.load(ignore_discard=True, ignore_expires=True)
      handlers.append(urllib.request.HTTPCookieProcessor(cj))
    except Exception:
      # If cookiejar parsing has formatting issues, fall back to curl
      return download_curl(url, cookies_file)

  opener = urllib.request.build_opener(*handlers)
  req = urllib.request.Request(
      url,
      headers={"User-Agent": "Mozilla/5.0 (v8-clusterfuzz-helper/1.0)"},
  )
  with opener.open(req, timeout=30) as resp:
    return resp.read()


def download_testcase(url: str, cookies_file: Path | None) -> bytes:
  """Downloads from ClusterFuzz with fallback between urllib and curl."""
  # Prefer curl when cookies are present for maximum compatibility with Netscape cookie files
  if cookies_file:
    return download_curl(url, cookies_file)
  try:
    return download_urllib(url, cookies_file)
  except Exception:
    return download_curl(url, cookies_file)


def parse_metadata_response(raw_output: bytes) -> dict:
  """Parses JSON response from testcase-detail/refresh endpoint."""
  if is_html_login_response(raw_output):
    raise PermissionError("Authentication required for metadata fetch")

  text = raw_output.decode("utf-8", errors="replace").strip()
  text = re.sub(r"^\)]}'\s*", "", text)

  try:
    data = json.loads(text)
  except json.JSONDecodeError as e:
    raise RuntimeError(f"Failed to parse JSON response: {e}")

  if isinstance(data, dict):
    if data.get("status") == 401 or data.get("type") == "UnauthorizedError":
      raise PermissionError(
          "Authentication required: ClusterFuzz returned UnauthorizedError (401)"
      )
    if "error" in data:
      raise RuntimeError(f"ClusterFuzz error: {data['error']}")
    if "testcase" not in data and "crash_type" not in data:
      raise RuntimeError(f"Invalid metadata response from ClusterFuzz: {data}")

  return data


def fetch_testcase_metadata(testcase_id: str,
                            cookies_file: Path | None) -> dict:
  """Queries the testcase-detail/refresh JSON endpoint to fetch report metadata."""
  cmd = [
      "curl",
      "-s",
      "-H",
      "Content-Type: application/json",
      "-d",
      json.dumps({"testcaseId": testcase_id}),
  ]
  if cookies_file:
    cmd.extend(["-b", str(cookies_file)])
  cmd.append(REFRESH_URL)

  res = subprocess.run(cmd, capture_output=True, check=False)
  if res.returncode != 0:
    err_msg = res.stderr.decode("utf-8", errors="replace").strip()
    raise RuntimeError(
        f"Metadata fetch failed (exit {res.returncode}): {err_msg}")

  return parse_metadata_response(res.stdout)


def map_job_to_local_out(job_type: str) -> str:
  """Maps a ClusterFuzz bot job type to the standard local V8 build output directory."""
  j = job_type.lower()
  if "asan" in j:
    return "out/x64.asan/d8"
  if "msan" in j:
    return "out/x64.msan/d8"
  if "tsan" in j:
    return "out/x64.tsan/d8"
  if "ubsan" in j:
    return "out/x64.ubsan/d8"
  if "optdebug" in j:
    return "out/x64.optdebug/d8"
  if "dbg" in j or "debug" in j:
    return "out/x64.debug/d8"
  return "out/x64.release/d8"


def display_metadata(data: dict) -> None:
  """Displays extracted ClusterFuzz metadata in a clean, human-readable format."""
  tc = data.get("testcase", {})
  crash_type = data.get("crash_type") or tc.get("crash_type") or "N/A"
  crash_state = data.get("crash_state") or tc.get("crash_state") or "N/A"
  job_type = tc.get("job_type") or "N/A"
  min_args = tc.get("minimized_arguments") or "N/A"
  win_arg = tc.get("window_argument") or ""
  crash_rev = data.get("crash_revision") or tc.get("crash_revision") or "N/A"
  regression = data.get("regression") or tc.get("regression") or "N/A"
  issue_url = data.get("issue_url") or "N/A"

  full_flags = min_args
  if win_arg and win_arg not in full_flags:
    full_flags = f"{full_flags} {win_arg}".strip()

  clean_regression = regression
  if "<" in regression and ">" in regression:
    clean_regression = re.sub(r"<br\s*/?>", "\n" + " " * 20, regression)
    clean_regression = re.sub(r"<[^>]+>", "", clean_regression).strip()

  local_d8 = map_job_to_local_out(job_type)

  print("=" * 70)
  print(f"ClusterFuzz Metadata (ID: {tc.get('id', 'N/A')})")
  print("=" * 70)
  print(f"Crash Type:         {crash_type}")
  print(
      f"Crash State:        {crash_state.splitlines()[0] if crash_state else 'N/A'}"
  )
  print(f"Job Type:           {job_type}")
  print(f"Crash Revision:     {crash_rev}")
  print(f"Regression Range:   {clean_regression}")
  print(f"Issue Tracker:      {issue_url}")
  print("-" * 70)
  print(f"Minimized d8 Flags: {min_args}")
  if win_arg:
    print(f"Window / Seed Flag: {win_arg}")
  print("-" * 70)
  print("Recommended Local Invocation:")
  print(f"timeout -v 15 {local_d8} {full_flags} <testcase.js>")
  print("=" * 70)


def main() -> int:
  parser = argparse.ArgumentParser(
      description="Download ClusterFuzz test cases and metadata from clusterfuzz.com."
  )
  parser.add_argument(
      "target",
      help="Testcase ID (e.g. 5197100195282944) or ClusterFuzz URL",
  )
  parser.add_argument(
      "-o",
      "--output",
      help="Output file path for reproducer script (defaults to a temporary file testcase_<id>.<ext>)",
  )
  parser.add_argument(
      "-c",
      "--cookies-file",
      help="Path to Netscape cookies.txt file for clusterfuzz.com",
  )
  parser.add_argument(
      "--info",
      "--metadata",
      action="store_true",
      dest="show_info",
      help="Fetch and display testcase report metadata (flags, job type, regression)",
  )
  parser.add_argument(
      "--save-metadata",
      help="Optional path to save full metadata JSON response",
  )

  args = parser.parse_args()

  try:
    testcase_id, url = parse_target(args.target)
    cookies_file = find_cookies_file(args.cookies_file)
  except (ValueError, FileNotFoundError) as e:
    print(f"Error: {e}", file=sys.stderr)
    return 1

  # Handle --info / --metadata mode
  if args.show_info or args.save_metadata:
    try:
      metadata = fetch_testcase_metadata(testcase_id, cookies_file)
      if args.show_info:
        display_metadata(metadata)
      if args.save_metadata:
        meta_path = Path(args.save_metadata).resolve()
        meta_path.parent.mkdir(parents=True, exist_ok=True)
        meta_path.write_text(json.dumps(metadata, indent=2))
        print(f"Saved full metadata report to: {meta_path}")
      return 0
    except PermissionError:
      print_missing_cookies_help()
      return 1
    except Exception as e:
      print(f"Error fetching metadata: {e}", file=sys.stderr)
      if not cookies_file:
        print_missing_cookies_help()
      return 1

  # Download mode
  output_path = args.output
  print(f"Fetching testcase from: {url}")
  if cookies_file:
    print(f"Using cookies file:     {cookies_file}")
  else:
    print("Notice: No cookies.txt found. Attempting unauthenticated download.")

  try:
    content = download_testcase(url, cookies_file)

    if not content:
      print("Warning: Received empty test case content.", file=sys.stderr)

    if is_html_login_response(content):
      print_missing_cookies_help()
      return 1

    is_wasm = content.startswith(b"\x00asm")
    if not output_path:
      if not re.fullmatch(r"\d+", testcase_id):
        raise ValueError(f"Invalid non-numeric testcase ID: {testcase_id!r}")
      temp_dir = Path(tempfile.gettempdir()).resolve()
      ext = ".wasm" if is_wasm else ".js"
      out_file = (temp_dir / f"testcase_{testcase_id}{ext}").resolve()
      if out_file.parent != temp_dir:
        raise ValueError(
            f"Refusing to write outside temp directory: {out_file}")
    else:
      out_file = Path(output_path).resolve()
      if is_wasm and out_file.suffix.lower() == ".js":
        print(
            "Notice: Test case content is a WebAssembly binary module (starts with \\x00asm), "
            f"but output path has .js extension: {out_file}",
            file=sys.stderr)

    out_file.parent.mkdir(parents=True, exist_ok=True)
    out_file.write_bytes(content)

    print(f"Successfully saved test case ({len(content)} bytes) to: {out_file}")
    if is_wasm:
      print("Notice: Downloaded test case is a WebAssembly binary module.")
      print("To convert it into readable JavaScript (WasmModuleBuilder), run:")
      print(f"  out/x64.release/wami --mjsunit {out_file} > repro.js")
    return 0

  except Exception as e:
    print(f"Error fetching testcase: {e}", file=sys.stderr)
    if not cookies_file:
      print_missing_cookies_help()
    return 1


if __name__ == "__main__":
  sys.exit(main())
