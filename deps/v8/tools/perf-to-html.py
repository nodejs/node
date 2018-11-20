#!/usr/bin/env python
# Copyright 2015 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''
python %prog

Convert a perf trybot JSON file into a pleasing HTML page. It can read
from standard input or via the --filename option. Examples:

  cat results.json | %prog --title "ia32 results"
  %prog -f results.json -t "ia32 results" -o results.html
'''

import json
import math
from optparse import OptionParser
import os
import shutil
import sys
import tempfile

PERCENT_CONSIDERED_SIGNIFICANT = 0.5
PROBABILITY_CONSIDERED_SIGNIFICANT = 0.02
PROBABILITY_CONSIDERED_MEANINGLESS = 0.05


def ComputeZ(baseline_avg, baseline_sigma, mean, n):
  if baseline_sigma == 0:
    return 1000.0;
  return abs((mean - baseline_avg) / (baseline_sigma / math.sqrt(n)))


# Values from http://www.fourmilab.ch/rpkp/experiments/analysis/zCalc.html
def ComputeProbability(z):
  if z > 2.575829: # p 0.005: two sided < 0.01
    return 0
  if z > 2.326348: # p 0.010
    return 0.01
  if z > 2.170091: # p 0.015
    return 0.02
  if z > 2.053749: # p 0.020
    return 0.03
  if z > 1.959964: # p 0.025: two sided < 0.05
    return 0.04
  if z > 1.880793: # p 0.030
    return 0.05
  if z > 1.811910: # p 0.035
    return 0.06
  if z > 1.750686: # p 0.040
    return 0.07
  if z > 1.695397: # p 0.045
    return 0.08
  if z > 1.644853: # p 0.050: two sided < 0.10
    return 0.09
  if z > 1.281551: # p 0.100: two sided < 0.20
    return 0.10
  return 0.20 # two sided p >= 0.20


class Result:
  def __init__(self, test_name, count, hasScoreUnits, result, sigma,
               master_result, master_sigma):
    self.result_ = float(result)
    self.sigma_ = float(sigma)
    self.master_result_ = float(master_result)
    self.master_sigma_ = float(master_sigma)
    self.significant_ = False
    self.notable_ = 0
    self.percentage_string_ = ""
    # compute notability and significance.
    try:
      if hasScoreUnits:
        compare_num = 100*self.result_/self.master_result_ - 100
      else:
        compare_num = 100*self.master_result_/self.result_ - 100
      if abs(compare_num) > 0.1:
        self.percentage_string_ = "%3.1f" % (compare_num)
        z = ComputeZ(self.master_result_, self.master_sigma_, self.result_, count)
        p = ComputeProbability(z)
        if p < PROBABILITY_CONSIDERED_SIGNIFICANT:
          self.significant_ = True
        if compare_num >= PERCENT_CONSIDERED_SIGNIFICANT:
          self.notable_ = 1
        elif compare_num <= -PERCENT_CONSIDERED_SIGNIFICANT:
          self.notable_ = -1
    except ZeroDivisionError:
      self.percentage_string_ = "NaN"
      self.significant_ = True

  def result(self):
    return self.result_

  def sigma(self):
    return self.sigma_

  def master_result(self):
    return self.master_result_

  def master_sigma(self):
    return self.master_sigma_

  def percentage_string(self):
    return self.percentage_string_;

  def isSignificant(self):
    return self.significant_

  def isNotablyPositive(self):
    return self.notable_ > 0

  def isNotablyNegative(self):
    return self.notable_ < 0


class Benchmark:
  def __init__(self, name, data):
    self.name_ = name
    self.tests_ = {}
    for test in data:
      # strip off "<name>/" prefix, allowing for subsequent "/"s
      test_name = test.split("/", 1)[1]
      self.appendResult(test_name, data[test])

  # tests is a dictionary of Results
  def tests(self):
    return self.tests_

  def SortedTestKeys(self):
    keys = self.tests_.keys()
    keys.sort()
    t = "Total"
    if t in keys:
      keys.remove(t)
      keys.append(t)
    return keys

  def name(self):
    return self.name_

  def appendResult(self, test_name, test_data):
    with_string = test_data["result with patch   "]
    data = with_string.split()
    master_string = test_data["result without patch"]
    master_data = master_string.split()
    runs = int(test_data["runs"])
    units = test_data["units"]
    hasScoreUnits = units == "score"
    self.tests_[test_name] = Result(test_name,
                                    runs,
                                    hasScoreUnits,
                                    data[0], data[2],
                                    master_data[0], master_data[2])


class BenchmarkRenderer:
  def __init__(self, output_file):
    self.print_output_ = []
    self.output_file_ = output_file

  def Print(self, str_data):
    self.print_output_.append(str_data)

  def FlushOutput(self):
    string_data = "\n".join(self.print_output_)
    print_output = []
    if self.output_file_:
      # create a file
      with open(self.output_file_, "w") as text_file:
        text_file.write(string_data)
    else:
      print(string_data)

  def RenderOneBenchmark(self, benchmark):
    self.Print("<h2>")
    self.Print("<a name=\"" + benchmark.name() + "\">")
    self.Print(benchmark.name() + "</a> <a href=\"#top\">(top)</a>")
    self.Print("</h2>");
    self.Print("<table class=\"benchmark\">")
    self.Print("<thead>")
    self.Print("  <th>Test</th>")
    self.Print("  <th>Result</th>")
    self.Print("  <th>Master</th>")
    self.Print("  <th>%</th>")
    self.Print("</thead>")
    self.Print("<tbody>")
    tests = benchmark.tests()
    for test in benchmark.SortedTestKeys():
      t = tests[test]
      self.Print("  <tr>")
      self.Print("    <td>" + test + "</td>")
      self.Print("    <td>" + str(t.result()) + "</td>")
      self.Print("    <td>" + str(t.master_result()) + "</td>")
      t = tests[test]
      res = t.percentage_string()
      if t.isSignificant():
        res = self.bold(res)
      if t.isNotablyPositive():
        res = self.green(res)
      elif t.isNotablyNegative():
        res = self.red(res)
      self.Print("    <td>" + res + "</td>")
      self.Print("  </tr>")
    self.Print("</tbody>")
    self.Print("</table>")

  def ProcessJSONData(self, data, title):
    self.Print("<h1>" + title + "</h1>")
    self.Print("<ul>")
    for benchmark in data:
     if benchmark != "errors":
       self.Print("<li><a href=\"#" + benchmark + "\">" + benchmark + "</a></li>")
    self.Print("</ul>")
    for benchmark in data:
      if benchmark != "errors":
        benchmark_object = Benchmark(benchmark, data[benchmark])
        self.RenderOneBenchmark(benchmark_object)

  def bold(self, data):
    return "<b>" + data + "</b>"

  def red(self, data):
    return "<font color=\"red\">" + data + "</font>"


  def green(self, data):
    return "<font color=\"green\">" + data + "</font>"

  def PrintHeader(self):
    data = """<html>
<head>
<title>Output</title>
<style type="text/css">
/*
Style inspired by Andy Ferra's gist at https://gist.github.com/andyferra/2554919
*/
body {
  font-family: Helvetica, arial, sans-serif;
  font-size: 14px;
  line-height: 1.6;
  padding-top: 10px;
  padding-bottom: 10px;
  background-color: white;
  padding: 30px;
}
h1, h2, h3, h4, h5, h6 {
  margin: 20px 0 10px;
  padding: 0;
  font-weight: bold;
  -webkit-font-smoothing: antialiased;
  cursor: text;
  position: relative;
}
h1 {
  font-size: 28px;
  color: black;
}

h2 {
  font-size: 24px;
  border-bottom: 1px solid #cccccc;
  color: black;
}

h3 {
  font-size: 18px;
}

h4 {
  font-size: 16px;
}

h5 {
  font-size: 14px;
}

h6 {
  color: #777777;
  font-size: 14px;
}

p, blockquote, ul, ol, dl, li, table, pre {
  margin: 15px 0;
}

li p.first {
  display: inline-block;
}

ul, ol {
  padding-left: 30px;
}

ul :first-child, ol :first-child {
  margin-top: 0;
}

ul :last-child, ol :last-child {
  margin-bottom: 0;
}

table {
  padding: 0;
}

table tr {
  border-top: 1px solid #cccccc;
  background-color: white;
  margin: 0;
  padding: 0;
}

table tr:nth-child(2n) {
  background-color: #f8f8f8;
}

table tr th {
  font-weight: bold;
  border: 1px solid #cccccc;
  text-align: left;
  margin: 0;
  padding: 6px 13px;
}
table tr td {
  border: 1px solid #cccccc;
  text-align: left;
  margin: 0;
  padding: 6px 13px;
}
table tr th :first-child, table tr td :first-child {
  margin-top: 0;
}
table tr th :last-child, table tr td :last-child {
  margin-bottom: 0;
}
</style>
</head>
<body>
"""
    self.Print(data)

  def PrintFooter(self):
    data = """</body>
</html>
"""
    self.Print(data)


def Render(opts, args):
  if opts.filename:
    with open(opts.filename) as json_data:
      data = json.load(json_data)
  else:
    # load data from stdin
    data = json.load(sys.stdin)

  if opts.title:
    title = opts.title
  elif opts.filename:
    title = opts.filename
  else:
    title = "Benchmark results"
  renderer = BenchmarkRenderer(opts.output)
  renderer.PrintHeader()
  renderer.ProcessJSONData(data, title)
  renderer.PrintFooter()
  renderer.FlushOutput()


if __name__ == '__main__':
  parser = OptionParser(usage=__doc__)
  parser.add_option("-f", "--filename", dest="filename",
                    help="Specifies the filename for the JSON results "
                         "rather than reading from stdin.")
  parser.add_option("-t", "--title", dest="title",
                    help="Optional title of the web page.")
  parser.add_option("-o", "--output", dest="output",
                    help="Write html output to this file rather than stdout.")

  (opts, args) = parser.parse_args()
  Render(opts, args)
