#!/usr/bin/env vpython3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import html
import re
import subprocess
import sys
from pathlib import Path
from urllib.parse import urlparse, unquote
import markdown
from markdown.treeprocessors import Treeprocessor
from markdown.extensions import Extension

REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
DOCUMENTATION_DIRECTORY = REPOSITORY_ROOT / 'docs'


class LinkExtractor(Treeprocessor):

  def __init__(self, md):
    super().__init__(md)
    self.links = []

  def run(self, root):
    for element in root.iter():
      if element.tag == 'a' and 'href' in element.attrib:
        self.links.append(element.attrib['href'])
      elif element.tag == 'img' and 'src' in element.attrib:
        self.links.append(element.attrib['src'])
    return None


class LinkExtractorExtension(Extension):

  def extendMarkdown(self, md):
    self.extractor = LinkExtractor(md)
    md.treeprocessors.register(self.extractor, 'link_extractor', 15)


def find_line_number_for_link(file_content, target_link):
  escaped_link_regex = re.escape(target_link)
  markdown_link_patterns = [
      rf'\({escaped_link_regex}\)', rf':\s*{escaped_link_regex}',
      escaped_link_regex
  ]
  for pattern in markdown_link_patterns:
    match = re.search(pattern, file_content)
    if match:
      return file_content.count('\n', 0, match.start()) + 1
  return 1


def is_path_valid(target_path, path_string):
  if target_path.exists():
    return True
  if not target_path.suffix and target_path.with_suffix('.md').exists():
    return True
  if path_string.startswith('/'):
    return False
  if (REPOSITORY_ROOT / path_string).exists():
    return True
  if not target_path.suffix and (REPOSITORY_ROOT /
                                 path_string).with_suffix('.md').exists():
    return True
  return False


def verify_link(link_url, file_content, markdown_file):
  unescaped_link_url = link_url.replace(markdown.util.AMP_SUBSTITUTE, '&')
  unescaped_link_url = html.unescape(unescaped_link_url)

  parsed_url = urlparse(unescaped_link_url)
  if parsed_url.scheme in ('http', 'https', 'mailto', 'ftp'):
    return None
  if not parsed_url.path:
    return None

  resolved_path_string = unquote(parsed_url.path)
  if resolved_path_string.startswith(('/blog/', '/_img/', '/bugs', '/bug')):
    return None

  if resolved_path_string.startswith('/'):
    resolved_target_path = REPOSITORY_ROOT / resolved_path_string.lstrip('/')
  else:
    resolved_target_path = (markdown_file.parent /
                            resolved_path_string).resolve()

  if is_path_valid(resolved_target_path, resolved_path_string):
    return None

  line_number = find_line_number_for_link(file_content, unescaped_link_url)

  relative_source_path = (
      markdown_file.relative_to(REPOSITORY_ROOT)
      if markdown_file.is_relative_to(REPOSITORY_ROOT) else markdown_file)

  relative_target_path = (
      resolved_target_path.relative_to(REPOSITORY_ROOT)
      if resolved_target_path.is_relative_to(REPOSITORY_ROOT) else
      resolved_target_path)

  return (f"{relative_source_path}:{line_number}: Broken link: "
          f"'{unescaped_link_url}' (resolved to '{relative_target_path}')")


def check_markdown_file(markdown_file_path):
  markdown_file = Path(markdown_file_path).resolve()
  if not markdown_file.exists():
    return [f"File not found: {markdown_file_path}"], []

  try:
    file_content = markdown_file.read_text(encoding='utf-8')
  except Exception as read_exception:
    return [f"Failed to read {markdown_file_path}: {read_exception}"], []

  parsing_errors = []
  link_errors = []

  extension = LinkExtractorExtension()
  try:
    md = markdown.Markdown(extensions=[extension])
    md.convert(file_content)
  except Exception as parse_exception:
    parsing_errors.append(
        f"{markdown_file_path}:0: Markdown parsing failed: {parse_exception}")
    return parsing_errors, []

  for link_url in extension.extractor.links:
    error = verify_link(link_url, file_content, markdown_file)
    if error:
      link_errors.append(error)

  return parsing_errors, link_errors


def get_all_tracked_markdown_files():
  try:
    result = subprocess.run(['git', 'ls-files', '*.md'],
                            capture_output=True,
                            text=True,
                            cwd=REPOSITORY_ROOT,
                            check=True)
    tracked_files = []
    for line in result.stdout.splitlines():
      line_stripped = line.strip()
      if line_stripped and not line_stripped.startswith('third_party/'):
        tracked_files.append(REPOSITORY_ROOT / line_stripped)
    return tracked_files
  except Exception:
    return list(DOCUMENTATION_DIRECTORY.rglob('*.md'))


def main():
  argument_parser = argparse.ArgumentParser(
      description="Verify markdown parsing and links.")
  argument_parser.add_argument(
      'files',
      nargs='*',
      help="Markdown files to check. If empty, checks all git-tracked markdown files in the repo."
  )
  argument_parser.add_argument(
      '-v',
      '--verbose',
      action='store_true',
      help="Print the currently processed file.")
  command_line_arguments = argument_parser.parse_args()

  if command_line_arguments.files:
    files_to_check = []
    for file_path in command_line_arguments.files:
      if file_path.endswith('.md'):
        files_to_check.append(Path(file_path))
  else:
    files_to_check = get_all_tracked_markdown_files()

  if not files_to_check:
    print("No markdown files found to check.")
    return 0

  all_parsing_errors = []
  all_link_errors = []
  for file_path in files_to_check:
    if command_line_arguments.verbose:
      print(f"Processing: {file_path}")
    parsing_errors, link_errors = check_markdown_file(file_path)
    all_parsing_errors.extend(parsing_errors)
    all_link_errors.extend(link_errors)

  if all_parsing_errors or all_link_errors:
    if all_parsing_errors:
      print("--- Markdown Parsing Errors ---", file=sys.stderr)
      print("\n".join(all_parsing_errors), file=sys.stderr)
    if all_link_errors:
      print("--- Markdown Link Validation Errors ---", file=sys.stderr)
      print("\n".join(all_link_errors), file=sys.stderr)
    return 1

  print(
      f"Successfully checked {len(files_to_check)} file(s). No parsing or link errors found."
  )
  return 0


if __name__ == '__main__':
  sys.exit(main())
