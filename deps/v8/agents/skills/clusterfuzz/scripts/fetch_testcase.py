#!/usr/bin/env python3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Fetches test cases and metadata from public ClusterFuzz (clusterfuzz.com).

Supports:
- Downloading reproducer scripts (.js or .wasm)
- Fetching issue metadata (minimized d8 flags, job type, regression range, crash stack)
- Optional OAuth Bearer token authentication via --oauth-token-cmd / CLUSTERFUZZ_OAUTH_TOKEN_CMD
- Automatic cookie jar fallback (~/.config/clusterfuzz/cookies.txt)
"""

from __future__ import annotations

import argparse
import http.cookiejar
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile
from urllib.error import HTTPError
import urllib.parse
import urllib.request

CLUSTERFUZZ_DOMAIN = "clusterfuzz.com"
DOWNLOAD_URL_FMT = "https://clusterfuzz.com/download?testcase_id={id}"
OAUTH_DOWNLOAD_URL_FMT = (
    "https://clusterfuzz.com/testcase-detail/download-testcase?id={id}")
REFRESH_URL = "https://clusterfuzz.com/testcase-detail/refresh"
OAUTH_TOKEN_CMD_ENV = "CLUSTERFUZZ_OAUTH_TOKEN_CMD"
USER_AGENT = "Mozilla/5.0 (v8-clusterfuzz-helper/1.0)"

COOKIE_EXTENSION_URL = (
    "https://chromewebstore.google.com/detail/cclelndahbckbenkjhflpdbgdldlbecc")


class _NoRedirect(urllib.request.HTTPRedirectHandler):
  """Prevents urllib from automatically forwarding headers across redirects."""

  def redirect_request(self, req, fp, code, msg, headers, newurl):
    return None


def print_missing_cookies_help() -> None:
  """Prints instructions for authenticating with clusterfuzz.com."""
  msg = f"""
================================================================================
AUTHENTICATION REQUIRED: Unable to authenticate with {CLUSTERFUZZ_DOMAIN}
================================================================================
ClusterFuzz returned a login page or rejected authentication/access.
To authenticate command-line downloads:
Option A (OAuth Bearer token command):
  Set {OAUTH_TOKEN_CMD_ENV} or pass --oauth-token-cmd to a command that prints
  an OAuth access token (plain text or JSON {{"token": "..."}}), and confirm
  your account has access to the bug or testcase on {CLUSTERFUZZ_DOMAIN}.

Option B (Browser cookies.txt):
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


def get_oauth_access_token(token_cmd: str | None = None) -> str | None:
  """Fetches an OAuth Bearer token by running a configured token command."""
  cmd_str = (token_cmd if token_cmd is not None else os.environ.get(
      OAUTH_TOKEN_CMD_ENV, "")).strip()
  if not cmd_str:
    return None

  try:
    argv = shlex.split(cmd_str)
  except ValueError as e:
    print(f"Note: Invalid OAuth token command: {e}", file=sys.stderr)
    return None
  if not argv:
    return None

  try:
    res = subprocess.run(
        argv,
        capture_output=True,
        text=True,
        timeout=30,
        check=False,
    )
  except (OSError, subprocess.SubprocessError) as e:
    print(f"Note: OAuth token command {argv[0]!r} failed: {e}", file=sys.stderr)
    return None

  if res.returncode != 0:
    stderr_lines = res.stderr.strip().splitlines()
    detail = stderr_lines[-1] if stderr_lines else f"exit code {res.returncode}"
    print(
        f"Note: OAuth token command {argv[0]!r} failed: {detail}",
        file=sys.stderr,
    )
    return None

  out = res.stdout.strip()
  if not out:
    return None

  if out.startswith("{"):
    try:
      parsed = json.loads(out)
    except ValueError:
      print(
          f"Note: Could not parse JSON output from {argv[0]!r}.",
          file=sys.stderr,
      )
      return None
    if isinstance(parsed, dict):
      token = parsed.get("token") or parsed.get("access_token")
      if isinstance(token, str):
        stripped = token.strip()
        if stripped and not any(ch.isspace() for ch in stripped):
          return stripped
    return None

  if not any(ch.isspace() for ch in out):
    return out
  print(
      f"Note: Unexpected whitespace in output from OAuth token command {argv[0]!r}.",
      file=sys.stderr,
  )
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


def _extract_http_error_detail(body: bytes) -> str:
  """Extracts a concise error summary from a ClusterFuzz HTTP error body."""
  text = body.decode("utf-8", errors="replace").strip()
  text = re.sub(r"^\)]}'\s*", "", text)
  try:
    parsed = json.loads(text)
    if isinstance(parsed, dict):
      err_type = parsed.get("type", "")
      msg = parsed.get("message") or parsed.get("error") or ""
      if err_type and msg:
        return f"{err_type}: {msg}"
      if msg:
        return str(msg)
      if err_type:
        return str(err_type)
  except ValueError:
    pass
  return text[:200]


def download_with_oauth(testcase_id: str,
                        access_token: str,
                        timeout: int = 30) -> bytes:
  """Downloads via ClusterFuzz's OAuth endpoint without leaking auth on redirects."""
  url = OAUTH_DOWNLOAD_URL_FMT.format(id=testcase_id)
  opener = urllib.request.build_opener(_NoRedirect)
  req = urllib.request.Request(
      url,
      headers={
          "Authorization": f"Bearer {access_token}",
          "User-Agent": USER_AGENT,
      },
  )
  try:
    with opener.open(req, timeout=timeout) as resp:
      content = resp.read()
      if is_html_login_response(content):
        raise PermissionError(
            "Authentication required: ClusterFuzz returned login HTML")
      return content
  except HTTPError as e:
    if e.code in (301, 302, 303, 307, 308):
      location = e.headers.get("Location", "") if e.headers else ""
      if not location:
        raise RuntimeError(
            f"ClusterFuzz redirect ({e.code}) missing Location header")
      redirect_url = urllib.parse.urljoin(url, location)
      parsed = urllib.parse.urlparse(redirect_url)
      if parsed.scheme not in ("http", "https"):
        raise RuntimeError(f"Refusing unsafe redirect URL: {redirect_url}")
      hostname = (parsed.hostname or "").lower()
      if ("/login" in parsed.path or hostname == "accounts.google.com" or
          hostname.endswith(".accounts.google.com")):
        raise PermissionError(
            "ClusterFuzz redirected to login; OAuth credentials rejected")
      # Follow the signed storage URL redirect without forwarding Authorization.
      follow_req = urllib.request.Request(
          redirect_url,
          headers={"User-Agent": USER_AGENT},
      )
      with urllib.request.urlopen(follow_req, timeout=timeout) as resp:
        return resp.read()

    detail = _extract_http_error_detail(e.read())
    if e.code in (401, 403):
      raise PermissionError(
          f"ClusterFuzz OAuth download failed (HTTP {e.code}): {detail}")
    raise RuntimeError(
        f"ClusterFuzz OAuth download failed (HTTP {e.code}): {detail}")


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
      headers={"User-Agent": USER_AGENT},
  )
  with opener.open(req, timeout=30) as resp:
    return resp.read()


def download_testcase(url: str,
                      cookies_file: Path | None,
                      access_token: str | None = None,
                      testcase_id: str | None = None) -> bytes:
  """Downloads from ClusterFuzz via OAuth (preferred) or cookies/unauthenticated."""
  if access_token:
    tc_id = testcase_id or parse_target(url)[0]
    try:
      return download_with_oauth(tc_id, access_token)
    except PermissionError as e:
      if not cookies_file:
        raise
      print(
          f"Note: OAuth download failed ({e}); falling back to cookies.",
          file=sys.stderr,
      )

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
    if (data.get("status") in (401, 403) or
        data.get("type") in ("UnauthorizedError", "AccessDeniedError")):
      err_type = data.get("type") or "AuthError"
      msg = data.get("message") or ""
      suffix = f": {msg}" if msg else ""
      raise PermissionError(
          f"Authentication/access error: ClusterFuzz returned {err_type} "
          f"({data.get('status', 'unknown')}){suffix}")
    if "error" in data:
      raise RuntimeError(f"ClusterFuzz error: {data['error']}")
    if "testcase" not in data and "crash_type" not in data:
      raise RuntimeError(f"Invalid metadata response from ClusterFuzz: {data}")

  return data


def fetch_metadata_with_oauth(testcase_id: str,
                              access_token: str,
                              timeout: int = 30) -> dict:
  """Queries /testcase-detail/refresh using an OAuth Bearer token."""
  opener = urllib.request.build_opener(_NoRedirect)
  body = json.dumps({"testcaseId": testcase_id}).encode("utf-8")
  req = urllib.request.Request(
      REFRESH_URL,
      data=body,
      headers={
          "Authorization": f"Bearer {access_token}",
          "Content-Type": "application/json",
          "User-Agent": USER_AGENT,
      },
  )
  try:
    with opener.open(req, timeout=timeout) as resp:
      return parse_metadata_response(resp.read())
  except HTTPError as e:
    if e.code in (301, 302, 303, 307, 308):
      raise PermissionError(
          f"ClusterFuzz redirected metadata request (HTTP {e.code}); "
          "OAuth credentials rejected")
    detail = _extract_http_error_detail(e.read())
    if e.code in (401, 403):
      raise PermissionError(
          f"ClusterFuzz OAuth metadata fetch failed (HTTP {e.code}): {detail}")
    raise RuntimeError(
        f"ClusterFuzz OAuth metadata fetch failed (HTTP {e.code}): {detail}")


def fetch_testcase_metadata(testcase_id: str,
                            cookies_file: Path | None,
                            access_token: str | None = None) -> dict:
  """Queries the testcase-detail/refresh JSON endpoint to fetch report metadata."""
  if access_token:
    try:
      return fetch_metadata_with_oauth(testcase_id, access_token)
    except PermissionError as e:
      if not cookies_file:
        raise
      print(
          f"Note: OAuth metadata fetch failed ({e}); falling back to cookies.",
          file=sys.stderr,
      )

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
      help="Path to Netscape cookies.txt file for clusterfuzz.com (disables OAuth)",
  )
  parser.add_argument(
      "--oauth-token-cmd",
      help=(
          "Command whose stdout provides an OAuth Bearer token (plain text or "
          "JSON with 'token'/'access_token'). Defaults to "
          f"${OAUTH_TOKEN_CMD_ENV}."),
  )
  parser.add_argument(
      "--no-oauth",
      action="store_true",
      help=f"Disable OAuth authentication even if {OAUTH_TOKEN_CMD_ENV} is set",
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

  # Prefer OAuth when configured unless disabled or --cookies-file was passed.
  access_token: str | None = None
  if not args.cookies_file and not args.no_oauth:
    access_token = get_oauth_access_token(args.oauth_token_cmd)

  # Handle --info / --metadata mode
  if args.show_info or args.save_metadata:
    try:
      metadata = fetch_testcase_metadata(
          testcase_id, cookies_file, access_token=access_token)
      if args.show_info:
        display_metadata(metadata)
      if args.save_metadata:
        meta_path = Path(args.save_metadata).resolve()
        meta_path.parent.mkdir(parents=True, exist_ok=True)
        meta_path.write_text(json.dumps(metadata, indent=2))
        print(f"Saved full metadata report to: {meta_path}")
      return 0
    except PermissionError as e:
      print(f"Error fetching metadata: {e}", file=sys.stderr)
      print_missing_cookies_help()
      return 1
    except Exception as e:
      print(f"Error fetching metadata: {e}", file=sys.stderr)
      if not access_token and not cookies_file:
        print_missing_cookies_help()
      return 1

  # Download mode
  output_path = args.output
  effective_url = (
      OAUTH_DOWNLOAD_URL_FMT.format(id=testcase_id) if access_token else url)
  print(f"Fetching testcase from: {effective_url}")
  if access_token:
    print("Using auth:             OAuth Bearer token")
    if cookies_file:
      print(f"Fallback cookies file:  {cookies_file}")
  elif cookies_file:
    print(f"Using cookies file:     {cookies_file}")
  else:
    print(
        "Notice: No OAuth token or cookies.txt found. Attempting unauthenticated download."
    )

  try:
    content = download_testcase(
        url, cookies_file, access_token=access_token, testcase_id=testcase_id)

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

  except PermissionError as e:
    print(f"Error fetching testcase: {e}", file=sys.stderr)
    print_missing_cookies_help()
    return 1
  except Exception as e:
    print(f"Error fetching testcase: {e}", file=sys.stderr)
    if not access_token and not cookies_file:
      print_missing_cookies_help()
    return 1


if __name__ == "__main__":
  sys.exit(main())
