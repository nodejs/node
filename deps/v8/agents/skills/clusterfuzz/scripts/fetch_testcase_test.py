#!/usr/bin/env python3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
from urllib.error import HTTPError

sys.path.insert(0, str(Path(__file__).resolve().parent))

from fetch_testcase import (
    OAUTH_TOKEN_CMD_ENV,
    download_testcase,
    download_with_oauth,
    fetch_metadata_with_oauth,
    fetch_testcase_metadata,
    find_cookies_file,
    get_oauth_access_token,
    is_html_login_response,
    main,
    map_job_to_local_out,
    parse_metadata_response,
    parse_target,
)


class _FakeResponse:

  def __init__(self, body: bytes):
    self._body = body

  def read(self) -> bytes:
    return self._body

  def __enter__(self):
    return self

  def __exit__(self, *args):
    return False


class FetchTestcaseTest(unittest.TestCase):

  def test_parse_target_numeric_id(self):
    tc_id, url = parse_target("5197100195282944")
    self.assertEqual(tc_id, "5197100195282944")
    self.assertEqual(
        url, "https://clusterfuzz.com/download?testcase_id=5197100195282944")

  def test_parse_target_public_url(self):
    url_in = "https://clusterfuzz.com/testcase-detail/5197100195282944"
    tc_id, url = parse_target(url_in)
    self.assertEqual(tc_id, "5197100195282944")
    self.assertEqual(
        url, "https://clusterfuzz.com/download?testcase_id=5197100195282944")

  def test_parse_target_public_query_param(self):
    url_in = "https://clusterfuzz.com/download?testcase_id=12345"
    tc_id, url = parse_target(url_in)
    self.assertEqual(tc_id, "12345")
    self.assertEqual(url, url_in)

  def test_parse_target_rejects_path_traversal(self):
    for bad_target in (
        "https://clusterfuzz.com/download?testcase_id=../../../etc/passwd",
        "https://clusterfuzz.com/download?testcase_id=%2e%2e%2f%2e%2e%2fpwned",
        "https://clusterfuzz.com/testcase?key=../pwned",
        "../../../etc/passwd",
        "https://clusterfuzz.com/testcase-detail/not_numeric",
    ):
      with self.subTest(target=bad_target):
        with self.assertRaises(ValueError):
          parse_target(bad_target)

  def test_parse_target_rejects_untrusted_host(self):
    for bad_url in (
        "https://attacker.example/download?testcase_id=12345",
        "http://127.0.0.1:8080/download?testcase_id=../../../tmp/pwned",
        "file:///etc/passwd?testcase_id=12345",
    ):
      with self.subTest(url=bad_url):
        with self.assertRaises(ValueError):
          parse_target(bad_url)

  def test_main_rejects_path_traversal_url(self):
    with tempfile.TemporaryDirectory() as d:
      target_file = Path(d) / "pwned.js"
      target_file_no_ext = str(target_file)[:-3]
      traversal = "../" * 10 + target_file_no_ext.lstrip("/")
      url = f"https://clusterfuzz.com/download?testcase_id={traversal}"
      with mock.patch.object(sys, "argv", ["fetch_testcase.py", url]):
        rc = main()
      self.assertEqual(rc, 1)
      self.assertFalse(target_file.exists())

  def test_map_job_to_local_out(self):
    self.assertEqual(map_job_to_local_out("linux_asan_d8"), "out/x64.asan/d8")
    self.assertEqual(map_job_to_local_out("linux_msan_d8"), "out/x64.msan/d8")
    self.assertEqual(map_job_to_local_out("linux_tsan_d8"), "out/x64.tsan/d8")
    self.assertEqual(
        map_job_to_local_out("linux_ubsan_vptr_d8"), "out/x64.ubsan/d8")
    self.assertEqual(map_job_to_local_out("linux_d8_dbg"), "out/x64.debug/d8")
    self.assertEqual(
        map_job_to_local_out("linux_d8_optdebug"), "out/x64.optdebug/d8")
    self.assertEqual(map_job_to_local_out("linux_d8_rel"), "out/x64.release/d8")
    self.assertEqual(map_job_to_local_out("v8_foozzie"), "out/x64.release/d8")

  def test_is_html_login_response(self):
    self.assertTrue(
        is_html_login_response(b"<!DOCTYPE html><html>Login</html>"))
    self.assertTrue(is_html_login_response(b"<html><head>"))
    self.assertTrue(
        is_html_login_response(b"window.location = '/session-login'"))
    self.assertFalse(is_html_login_response(b"print('hello world');"))
    self.assertFalse(is_html_login_response(b"\x00asm\x01\x00\x00\x00"))

  def test_parse_metadata_response_valid(self):
    raw = b')]}\'\n{"testcase": {"id": 12345, "job_type": "linux_asan_d8"}}'
    data = parse_metadata_response(raw)
    self.assertEqual(data["testcase"]["id"], 12345)
    self.assertEqual(data["testcase"]["job_type"], "linux_asan_d8")

  def test_parse_metadata_response_unauthorized(self):
    raw = b'{"status": 401, "type": "UnauthorizedError"}'
    with self.assertRaises(PermissionError):
      parse_metadata_response(raw)

  def test_parse_metadata_response_access_denied(self):
    raw = b'{"status": 403, "type": "AccessDeniedError", "message": "Denied"}'
    with self.assertRaisesRegex(PermissionError, "AccessDeniedError.*Denied"):
      parse_metadata_response(raw)

  def test_find_cookies_file_specified(self):
    with tempfile.NamedTemporaryFile() as tmp:
      found = find_cookies_file(tmp.name)
      self.assertEqual(found, Path(tmp.name).resolve())

    with self.assertRaises(FileNotFoundError):
      find_cookies_file("/non/existent/path/cookies.txt")

  def test_find_cookies_file_default_only_uses_config_dir(self):
    with tempfile.TemporaryDirectory() as fake_home:
      with mock.patch.object(Path, "home", return_value=Path(fake_home)):
        self.assertIsNone(find_cookies_file())
        config_cookies = Path(
            fake_home) / ".config" / "clusterfuzz" / "cookies.txt"
        config_cookies.parent.mkdir(parents=True)
        config_cookies.write_text("# Netscape HTTP Cookie File\n")
        self.assertEqual(find_cookies_file(), config_cookies)

  def test_get_oauth_access_token_unset_returns_none(self):
    with mock.patch.dict(os.environ, {}, clear=True):
      self.assertIsNone(get_oauth_access_token())
    self.assertIsNone(get_oauth_access_token(""))

  def test_get_oauth_access_token_json_token_success(self):
    proc = subprocess.CompletedProcess(
        args=["token-helper", "--json"],
        returncode=0,
        stdout=json.dumps({"token": "fake-oauth-token"}),
        stderr="",
    )
    with mock.patch("fetch_testcase.subprocess.run", return_value=proc) as m:
      token = get_oauth_access_token("token-helper --json")
    self.assertEqual(token, "fake-oauth-token")
    m.assert_called_once()
    self.assertEqual(m.call_args[0][0], ["token-helper", "--json"])

  def test_get_oauth_access_token_json_access_token_from_env(self):
    proc = subprocess.CompletedProcess(
        args=["my-helper"],
        returncode=0,
        stdout=json.dumps({"access_token": "fake-access-token"}),
        stderr="",
    )
    with mock.patch.dict(
        os.environ, {OAUTH_TOKEN_CMD_ENV: "my-helper"},
        clear=False), mock.patch(
            "fetch_testcase.subprocess.run", return_value=proc):
      token = get_oauth_access_token()
    self.assertEqual(token, "fake-access-token")

  def test_get_oauth_access_token_plain_text_success(self):
    proc = subprocess.CompletedProcess(
        args=["gcloud", "auth", "print-access-token"],
        returncode=0,
        stdout="fake-plain-token\n",
        stderr="",
    )
    with mock.patch("fetch_testcase.subprocess.run", return_value=proc):
      token = get_oauth_access_token("gcloud auth print-access-token")
    self.assertEqual(token, "fake-plain-token")

  def test_get_oauth_access_token_rejects_whitespace_output(self):
    proc = subprocess.CompletedProcess(
        args=["broken-helper"],
        returncode=0,
        stdout="error: please login first\n",
        stderr="",
    )
    with mock.patch("fetch_testcase.subprocess.run", return_value=proc):
      self.assertIsNone(get_oauth_access_token("broken-helper"))

  def test_get_oauth_access_token_failure_returns_none(self):
    proc = subprocess.CompletedProcess(
        args=["token-helper"],
        returncode=1,
        stdout="",
        stderr="credentials expired",
    )
    with mock.patch("fetch_testcase.subprocess.run", return_value=proc):
      self.assertIsNone(get_oauth_access_token("token-helper"))

  def test_get_oauth_access_token_missing_binary_returns_none(self):
    self.assertIsNone(get_oauth_access_token("/nonexistent/token-helper"))

  def test_download_with_oauth_strips_auth_on_gcs_redirect(self):
    redirect_err = HTTPError(
        url="https://clusterfuzz.com/testcase-detail/download-testcase?id=123",
        code=302,
        msg="Found",
        hdrs={
            "Location":
                "https://storage.googleapis.com/clusterfuzz-blobs/abc?Signature=xyz"
        },
        fp=io.BytesIO(b""),
    )
    fake_opener = mock.MagicMock()
    fake_opener.open.side_effect = redirect_err

    with mock.patch(
        "fetch_testcase.urllib.request.build_opener",
        return_value=fake_opener), mock.patch(
            "fetch_testcase.urllib.request.urlopen",
            return_value=_FakeResponse(b"print(42);\n")) as mock_urlopen:
      content = download_with_oauth("123", "secret-token")

    self.assertEqual(content, b"print(42);\n")
    first_req = fake_opener.open.call_args[0][0]
    self.assertEqual(
        first_req.full_url,
        "https://clusterfuzz.com/testcase-detail/download-testcase?id=123")
    self.assertEqual(
        first_req.get_header("Authorization"), "Bearer secret-token")

    follow_req = mock_urlopen.call_args[0][0]
    self.assertEqual(
        follow_req.full_url,
        "https://storage.googleapis.com/clusterfuzz-blobs/abc?Signature=xyz")
    self.assertIsNone(follow_req.get_header("Authorization"))

  def test_download_with_oauth_rejects_login_redirect(self):
    redirect_err = HTTPError(
        url="https://clusterfuzz.com/testcase-detail/download-testcase?id=123",
        code=302,
        msg="Found",
        hdrs={"Location": "https://clusterfuzz.com/login?dest=..."},
        fp=io.BytesIO(b""),
    )
    fake_opener = mock.MagicMock()
    fake_opener.open.side_effect = redirect_err

    with mock.patch(
        "fetch_testcase.urllib.request.build_opener", return_value=fake_opener):
      with self.assertRaises(PermissionError):
        download_with_oauth("123", "secret-token")

  def test_download_with_oauth_rejects_non_http_redirect(self):
    redirect_err = HTTPError(
        url="https://clusterfuzz.com/testcase-detail/download-testcase?id=123",
        code=302,
        msg="Found",
        hdrs={"Location": "file:///etc/passwd"},
        fp=io.BytesIO(b""),
    )
    fake_opener = mock.MagicMock()
    fake_opener.open.side_effect = redirect_err

    with mock.patch(
        "fetch_testcase.urllib.request.build_opener", return_value=fake_opener):
      with self.assertRaisesRegex(RuntimeError, "unsafe redirect"):
        download_with_oauth("123", "secret-token")

  def test_download_with_oauth_401_raises_permission_error(self):
    err_body = json.dumps({
        "status":
            401,
        "type":
            "UnauthorizedError",
        "message":
            "The access token doesn't belong to one of the allowed OAuth clients.",
    }).encode()
    http_err = HTTPError(
        url="https://clusterfuzz.com/testcase-detail/download-testcase?id=123",
        code=401,
        msg="Unauthorized",
        hdrs={},
        fp=io.BytesIO(err_body),
    )
    fake_opener = mock.MagicMock()
    fake_opener.open.side_effect = http_err

    with mock.patch(
        "fetch_testcase.urllib.request.build_opener", return_value=fake_opener):
      with self.assertRaisesRegex(PermissionError, "allowed OAuth clients"):
        download_with_oauth("123", "secret-token")

  def test_fetch_metadata_with_oauth_success(self):
    fake_opener = mock.MagicMock()
    fake_opener.open.return_value = _FakeResponse(
        b'{"testcase": {"id": 999, "job_type": "linux_asan_d8"}}')

    with mock.patch(
        "fetch_testcase.urllib.request.build_opener", return_value=fake_opener):
      data = fetch_metadata_with_oauth("999", "tok-123")

    self.assertEqual(data["testcase"]["id"], 999)
    req = fake_opener.open.call_args[0][0]
    self.assertEqual(req.get_header("Authorization"), "Bearer tok-123")
    self.assertEqual(
        json.loads(req.data.decode("utf-8")), {"testcaseId": "999"})

  def test_download_testcase_falls_back_to_cookies_on_oauth_permission_error(
      self):
    url = "https://clusterfuzz.com/download?testcase_id=123"
    cookies = Path("/tmp/fake_cookies.txt")
    with mock.patch(
        "fetch_testcase.download_with_oauth",
        side_effect=PermissionError("403")), mock.patch(
            "fetch_testcase.download_curl",
            return_value=b"fallback-js") as mock_curl:
      content = download_testcase(url, cookies, access_token="tok-123")
    self.assertEqual(content, b"fallback-js")
    mock_curl.assert_called_once_with(url, cookies)

  def test_download_testcase_raises_oauth_permission_error_without_cookies(
      self):
    url = "https://clusterfuzz.com/download?testcase_id=123"
    with mock.patch(
        "fetch_testcase.download_with_oauth",
        side_effect=PermissionError("401 disallowed client")):
      with self.assertRaisesRegex(PermissionError, "401 disallowed client"):
        download_testcase(url, None, access_token="tok-123")

  def test_fetch_testcase_metadata_falls_back_to_cookies(self):
    cookies = Path("/tmp/fake_cookies.txt")
    proc = subprocess.CompletedProcess(
        args=["curl"],
        returncode=0,
        stdout=b'{"testcase": {"id": 123, "job_type": "linux_d8_dbg"}}',
        stderr=b"",
    )
    with mock.patch(
        "fetch_testcase.fetch_metadata_with_oauth",
        side_effect=PermissionError("403")), mock.patch(
            "fetch_testcase.subprocess.run", return_value=proc):
      data = fetch_testcase_metadata("123", cookies, access_token="tok-123")
    self.assertEqual(data["testcase"]["id"], 123)

  def test_main_explicit_cookies_file_skips_oauth(self):
    with tempfile.NamedTemporaryFile() as tmp_cookies, \
        tempfile.TemporaryDirectory() as tmp_dir:
      out_path = Path(tmp_dir) / "poc.js"
      with mock.patch.object(
          sys,
          "argv",
          [
              "fetch_testcase.py",
              "12345",
              "-c",
              tmp_cookies.name,
              "-o",
              str(out_path),
          ],
      ), mock.patch("fetch_testcase.get_oauth_access_token") as mock_oauth, \
          mock.patch(
              "fetch_testcase.download_testcase",
              return_value=b"print(1);\n") as mock_dl:
        rc = main()
      self.assertEqual(rc, 0)
      mock_oauth.assert_not_called()
      self.assertIsNone(mock_dl.call_args.kwargs["access_token"])
      self.assertEqual(out_path.read_bytes(), b"print(1);\n")

  def test_main_no_oauth_flag_skips_oauth(self):
    with tempfile.TemporaryDirectory() as tmp_dir:
      out_path = Path(tmp_dir) / "poc.js"
      with mock.patch.object(
          sys,
          "argv",
          ["fetch_testcase.py", "12345", "--no-oauth", "-o",
           str(out_path)],
      ), mock.patch("fetch_testcase.find_cookies_file", return_value=None), \
          mock.patch("fetch_testcase.get_oauth_access_token") as mock_oauth, \
          mock.patch(
              "fetch_testcase.download_testcase",
              return_value=b"print(1);\n") as mock_dl:
        rc = main()
      self.assertEqual(rc, 0)
      mock_oauth.assert_not_called()
      self.assertIsNone(mock_dl.call_args.kwargs["access_token"])

  def test_main_oauth_token_cmd_passed_to_helper(self):
    with tempfile.TemporaryDirectory() as tmp_dir:
      out_path = Path(tmp_dir) / "poc.js"
      with mock.patch.object(
          sys,
          "argv",
          [
              "fetch_testcase.py",
              "12345",
              "--oauth-token-cmd",
              "custom-helper --flag",
              "-o",
              str(out_path),
          ],
      ), mock.patch("fetch_testcase.find_cookies_file", return_value=None), \
          mock.patch(
              "fetch_testcase.get_oauth_access_token",
              return_value="tok-abc") as mock_oauth, mock.patch(
                  "fetch_testcase.download_testcase",
                  return_value=b"print(2);\n") as mock_dl:
        rc = main()
      self.assertEqual(rc, 0)
      mock_oauth.assert_called_once_with("custom-helper --flag")
      self.assertEqual(mock_dl.call_args.kwargs["access_token"], "tok-abc")
      self.assertEqual(out_path.read_bytes(), b"print(2);\n")

  def test_regression_html_stripping(self):
    import re
    regression = 'V8: <a target="_blank" href="https://example.com">81576:81577</a><br />Common: 12345'
    clean = regression
    if "<" in regression and ">" in regression:
      clean = re.sub(r"<br\s*/?>", "\n" + " " * 20, regression)
      clean = re.sub(r"<[^>]+>", "", clean).strip()
    self.assertEqual(clean,
                     "V8: 81576:81577\n                    Common: 12345")


if __name__ == "__main__":
  unittest.main()
