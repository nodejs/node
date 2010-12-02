#!/usr/bin/env python
#
# Copyright 2007 The Closure Linter Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS-IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Utilities for dealing with HTML."""

__author__ = ('robbyw@google.com (Robert Walker)')

import cStringIO
import formatter
import htmllib
import HTMLParser
import re


class ScriptExtractor(htmllib.HTMLParser):
  """Subclass of HTMLParser that extracts script contents from an HTML file.

  Also inserts appropriate blank lines so that line numbers in the extracted
  code match the line numbers in the original HTML.
  """

  def __init__(self):
    """Initialize a ScriptExtractor."""
    htmllib.HTMLParser.__init__(self, formatter.NullFormatter())
    self._in_script = False
    self._text = ''

  def start_script(self, attrs):
    """Internal handler for the start of a script tag.

    Args:
      attrs: The attributes of the script tag, as a list of tuples.
    """
    for attribute in attrs:
      if attribute[0].lower() == 'src':
        # Skip script tags with a src specified.
        return
    self._in_script = True

  def end_script(self):
    """Internal handler for the end of a script tag."""
    self._in_script = False

  def handle_data(self, data):
    """Internal handler for character data.

    Args:
      data: The character data from the HTML file.
    """
    if self._in_script:
      # If the last line contains whitespace only, i.e. is just there to
      # properly align a </script> tag, strip the whitespace.
      if data.rstrip(' \t') != data.rstrip(' \t\n\r\f'):
        data = data.rstrip(' \t')
      self._text += data
    else:
      self._AppendNewlines(data)

  def handle_comment(self, data):
    """Internal handler for HTML comments.

    Args:
      data: The text of the comment.
    """
    self._AppendNewlines(data)

  def _AppendNewlines(self, data):
    """Count the number of newlines in the given string and append them.

    This ensures line numbers are correct for reported errors.

    Args:
      data: The data to count newlines in.
    """
    # We append 'x' to both sides of the string to ensure that splitlines
    # gives us an accurate count.
    for i in xrange(len(('x' + data + 'x').splitlines()) - 1):
      self._text += '\n'

  def GetScriptLines(self):
    """Return the extracted script lines.

    Returns:
      The extracted script lines as a list of strings.
    """
    return self._text.splitlines()


def GetScriptLines(f):
  """Extract script tag contents from the given HTML file.

  Args:
    f: The HTML file.

  Returns:
    Lines in the HTML file that are from script tags.
  """
  extractor = ScriptExtractor()

  # The HTML parser chokes on text like Array.<!string>, so we patch
  # that bug by replacing the < with &lt; - escaping all text inside script
  # tags would be better but it's a bit of a catch 22.
  contents = f.read()
  contents = re.sub(r'<([^\s\w/])',
         lambda x: '&lt;%s' % x.group(1),
         contents)

  extractor.feed(contents)
  extractor.close()
  return extractor.GetScriptLines()


def StripTags(str):
  """Returns the string with HTML tags stripped.

  Args:
    str: An html string.

  Returns:
    The html string with all tags stripped. If there was a parse error, returns
    the text successfully parsed so far.
  """
  # Brute force approach to stripping as much HTML as possible. If there is a
  # parsing error, don't strip text before parse error position, and continue
  # trying from there.
  final_text = ''
  finished = False
  while not finished:
    try:
      strip = _HtmlStripper()
      strip.feed(str)
      strip.close()
      str = strip.get_output()
      final_text += str
      finished = True
    except HTMLParser.HTMLParseError, e:
      final_text += str[:e.offset]
      str = str[e.offset + 1:]

  return final_text


class _HtmlStripper(HTMLParser.HTMLParser):
  """Simple class to strip tags from HTML.

  Does so by doing nothing when encountering tags, and appending character data
  to a buffer when that is encountered.
  """
  def __init__(self):
    self.reset()
    self.__output = cStringIO.StringIO()

  def handle_data(self, d):
    self.__output.write(d)

  def get_output(self):
    return self.__output.getvalue()
