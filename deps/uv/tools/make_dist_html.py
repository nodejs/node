#!/usr/bin/python

from __future__ import print_function

import itertools
import os
import re
import subprocess

HTML = r'''
<!DOCTYPE html>
<html>
  <head>
    <link rel="stylesheet" href="http://libuv.org/styles/vendor.css">
    <link rel="stylesheet" href="http://libuv.org/styles/main.css">
    <style>
    table {{
      border-spacing: 0;
    }}
    body table {{
      margin: 0 0 0 12pt;
    }}
    th, td {{
      padding: 2pt;
      text-align: left;
      vertical-align: top;
    }}
    table table {{
      border-collapse: initial;
      padding: 0 0 16pt 0;
    }}
    table table tr:nth-child(even) {{
      background-color: #777;
    }}
    </style>
  </head>
  <body>
    <table>{groups}</table>
  </body>
</html>
'''

GROUPS = r'''
<tr>
  <td>{groups[0]}</td>
  <td>{groups[1]}</td>
  <td>{groups[2]}</td>
  <td>{groups[3]}</td>
</tr>
'''

GROUP = r'''
<table>
  <tr>
    <th>version</th>
    <th>tarball</th>
    <th>gpg</th>
    <th>windows</th>
  </tr>
  {rows}
</table>
'''

ROW = r'''
<tr>
  <td>
    <a href="http://dist.libuv.org/dist/{tag}/">{tag}</a>
  </td>
  <td>
    <a href="http://dist.libuv.org/dist/{tag}/libuv-{tag}.tar.gz">tarball</a>
  </td>
  <td>{maybe_gpg}</td>
  <td>{maybe_exe}</td>
</tr>
'''

GPG = r'''
<a href="http://dist.libuv.org/dist/{tag}/libuv-{tag}.tar.gz.sign">gpg</a>
'''

# The binaries don't have a predictable name, link to the directory instead.
EXE = r'''
<a href="http://dist.libuv.org/dist/{tag}/">exe</a>
'''

def version(tag):
  return map(int, re.match('^v(\d+)\.(\d+)\.(\d+)', tag).groups())

def major_minor(tag):
  return version(tag)[:2]

def row_for(tag):
  maybe_gpg = ''
  maybe_exe = ''
  # We didn't start signing releases and producing Windows installers
  # until v1.7.0.
  if version(tag) >= version('v1.7.0'):
    maybe_gpg = GPG.format(**locals())
    maybe_exe = EXE.format(**locals())
  return ROW.format(**locals())

def group_for(tags):
  rows = ''.join(row_for(tag) for tag in tags)
  return GROUP.format(rows=rows)

# Partition in groups of |n|.
def groups_for(groups, n=4):
  html = ''
  groups = groups[:] + [''] * (n - 1)
  while len(groups) >= n:
    html += GROUPS.format(groups=groups)
    groups = groups[n:]
  return html

if __name__ == '__main__':
  os.chdir(os.path.dirname(__file__))
  tags = subprocess.check_output(['git', 'tag'])
  tags = [tag for tag in tags.split('\n') if tag.startswith('v')]
  tags.sort(key=version, reverse=True)
  groups = [group_for(list(g)) for _, g in itertools.groupby(tags, major_minor)]
  groups = groups_for(groups)
  html = HTML.format(groups=groups).strip()
  html = re.sub('>\\s+<', '><', html)
  print(html)
