#!/usr/bin/env python
# Copyright 2017 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''
python %prog

Compare perf trybot JSON files and output the results into a pleasing HTML page.
Examples:
  %prog -t "ia32 results" Result,../result.json Master,/path-to/master.json -o results.html
  %prog -t "x64 results" ../result.json master.json -o results.html
'''

from collections import OrderedDict
import json
import math
from argparse import ArgumentParser
import os
import shutil
import sys
import tempfile

PERCENT_CONSIDERED_SIGNIFICANT = 0.5
PROBABILITY_CONSIDERED_SIGNIFICANT = 0.02
PROBABILITY_CONSIDERED_MEANINGLESS = 0.05

class Statistics:
  @staticmethod
  def Mean(values):
    return float(sum(values)) / len(values)

  @staticmethod
  def Variance(values, average):
    return map(lambda x: (x - average) ** 2, values)

  @staticmethod
  def StandardDeviation(values, average):
    return math.sqrt(Statistics.Mean(Statistics.Variance(values, average)))

  @staticmethod
  def ComputeZ(baseline_avg, baseline_sigma, mean, n):
    if baseline_sigma == 0:
      return 1000.0;
    return abs((mean - baseline_avg) / (baseline_sigma / math.sqrt(n)))

  # Values from http://www.fourmilab.ch/rpkp/experiments/analysis/zCalc.html
  @staticmethod
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


class ResultsDiff:
  def __init__(self, significant, notable, percentage_string):
    self.significant_ = significant
    self.notable_ = notable
    self.percentage_string_ = percentage_string

  def percentage_string(self):
    return self.percentage_string_;

  def isSignificant(self):
    return self.significant_

  def isNotablyPositive(self):
    return self.notable_ > 0

  def isNotablyNegative(self):
    return self.notable_ < 0


class BenchmarkResult:
  def __init__(self, units, count, result, sigma):
    self.units_ = units
    self.count_ = float(count)
    self.result_ = float(result)
    self.sigma_ = float(sigma)

  def Compare(self, other):
    if self.units_ != other.units_:
      print ("Incompatible units: %s and %s" % (self.units_, other.units_))
      sys.exit(1)

    significant = False
    notable = 0
    percentage_string = ""
    # compute notability and significance.
    if self.units_ == "score":
      compare_num = 100*self.result_/other.result_ - 100
    else:
      compare_num = 100*other.result_/self.result_ - 100
    if abs(compare_num) > 0.1:
      percentage_string = "%3.1f" % (compare_num)
      z = Statistics.ComputeZ(other.result_, other.sigma_,
                              self.result_, self.count_)
      p = Statistics.ComputeProbability(z)
      if p < PROBABILITY_CONSIDERED_SIGNIFICANT:
        significant = True
      if compare_num >= PERCENT_CONSIDERED_SIGNIFICANT:
        notable = 1
      elif compare_num <= -PERCENT_CONSIDERED_SIGNIFICANT:
        notable = -1
    return ResultsDiff(significant, notable, percentage_string)

  def result(self):
    return self.result_

  def sigma(self):
    return self.sigma_


class Benchmark:
  def __init__(self, name):
    self.name_ = name
    self.runs_ = {}

  def name(self):
    return self.name_

  def getResult(self, run_name):
    return self.runs_.get(run_name)

  def appendResult(self, run_name, trace):
    values = map(float, trace['results'])
    count = len(values)
    mean = Statistics.Mean(values)
    stddev = float(trace.get('stddev') or
                   Statistics.StandardDeviation(values, mean))
    units = trace["units"]
    # print run_name, units, count, mean, stddev
    self.runs_[run_name] = BenchmarkResult(units, count, mean, stddev)


class BenchmarkSuite:
  def __init__(self, name):
    self.name_ = name
    self.benchmarks_ = {}

  def SortedTestKeys(self):
    keys = self.benchmarks_.keys()
    keys.sort()
    t = "Total"
    if t in keys:
      keys.remove(t)
      keys.append(t)
    return keys

  def name(self):
    return self.name_

  def getBenchmark(self, benchmark_name):
    benchmark_object = self.benchmarks_.get(benchmark_name)
    if benchmark_object == None:
      benchmark_object = Benchmark(benchmark_name)
      self.benchmarks_[benchmark_name] = benchmark_object
    return benchmark_object


class ResultTableRenderer:
  def __init__(self, output_file):
    self.benchmarks_ = []
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

  def bold(self, data):
    return "<b>%s</b>" % data

  def red(self, data):
    return "<font color=\"red\">%s</font>" % data


  def green(self, data):
    return "<font color=\"green\">%s</font>" % data

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
  text-align: right;
  margin: 0;
  padding: 6px 13px;
}
table tr td.name-column {
  text-align: left;
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

  def StartSuite(self, suite_name, run_names):
    self.Print("<h2>")
    self.Print("<a name=\"%s\">%s</a> <a href=\"#top\">(top)</a>" %
               (suite_name, suite_name))
    self.Print("</h2>");
    self.Print("<table class=\"benchmark\">")
    self.Print("<thead>")
    self.Print("  <th>Test</th>")
    main_run = None
    for run_name in run_names:
      self.Print("  <th>%s</th>" % run_name)
      if main_run == None:
        main_run = run_name
      else:
        self.Print("  <th>%</th>")
    self.Print("</thead>")
    self.Print("<tbody>")


  def FinishSuite(self):
    self.Print("</tbody>")
    self.Print("</table>")


  def StartBenchmark(self, benchmark_name):
    self.Print("  <tr>")
    self.Print("    <td class=\"name-column\">%s</td>" % benchmark_name)

  def FinishBenchmark(self):
    self.Print("  </tr>")


  def PrintResult(self, run):
    if run == None:
      self.PrintEmptyCell()
      return
    self.Print("    <td>%3.1f</td>" % run.result())


  def PrintComparison(self, run, main_run):
    if run == None or main_run == None:
      self.PrintEmptyCell()
      return
    diff = run.Compare(main_run)
    res = diff.percentage_string()
    if diff.isSignificant():
      res = self.bold(res)
    if diff.isNotablyPositive():
      res = self.green(res)
    elif diff.isNotablyNegative():
      res = self.red(res)
    self.Print("    <td>%s</td>" % res)


  def PrintEmptyCell(self):
    self.Print("    <td></td>")


  def StartTOC(self, title):
    self.Print("<h1>%s</h1>" % title)
    self.Print("<ul>")

  def FinishTOC(self):
    self.Print("</ul>")

  def PrintBenchmarkLink(self, benchmark):
    self.Print("<li><a href=\"#" + benchmark + "\">" + benchmark + "</a></li>")

  def PrintFooter(self):
    data = """</body>
</html>
"""
    self.Print(data)


def Render(args):
  benchmark_suites = {}
  run_names = OrderedDict()

  for json_file_list in args.json_file_list:
    run_name = json_file_list[0]
    if run_name.endswith(".json"):
      # The first item in the list is also a file name
      run_name = os.path.splitext(run_name)[0]
      filenames = json_file_list
    else:
      filenames = json_file_list[1:]

    for filename in filenames:
      print ("Processing result set \"%s\", file: %s" % (run_name, filename))
      with open(filename) as json_data:
        data = json.load(json_data)

      run_names[run_name] = 0

      for error in data["errors"]:
        print "Error:", error

      for trace in data["traces"]:
        suite_name = trace["graphs"][0]
        benchmark_name = "/".join(trace["graphs"][1:])

        benchmark_suite_object = benchmark_suites.get(suite_name)
        if benchmark_suite_object == None:
          benchmark_suite_object = BenchmarkSuite(suite_name)
          benchmark_suites[suite_name] = benchmark_suite_object

        benchmark_object = benchmark_suite_object.getBenchmark(benchmark_name)
        benchmark_object.appendResult(run_name, trace);


  renderer = ResultTableRenderer(args.output)
  renderer.PrintHeader()

  title = args.title or "Benchmark results"
  renderer.StartTOC(title)
  for suite_name, benchmark_suite_object in sorted(benchmark_suites.iteritems()):
    renderer.PrintBenchmarkLink(suite_name)
  renderer.FinishTOC()

  for suite_name, benchmark_suite_object in sorted(benchmark_suites.iteritems()):
    renderer.StartSuite(suite_name, run_names)
    for benchmark_name in benchmark_suite_object.SortedTestKeys():
      benchmark_object = benchmark_suite_object.getBenchmark(benchmark_name)
      # print suite_name, benchmark_object.name()

      renderer.StartBenchmark(benchmark_name)
      main_run = None
      main_result = None
      for run_name in run_names:
        result = benchmark_object.getResult(run_name)
        renderer.PrintResult(result)
        if main_run == None:
          main_run = run_name
          main_result = result
        else:
          renderer.PrintComparison(result, main_result)
      renderer.FinishBenchmark()
    renderer.FinishSuite()

  renderer.PrintFooter()
  renderer.FlushOutput()

def CommaSeparatedList(arg):
  return [x for x in arg.split(',')]

if __name__ == '__main__':
  parser = ArgumentParser(description="Compare perf trybot JSON files and " +
                          "output the results into a pleasing HTML page.")
  parser.add_argument("-t", "--title", dest="title",
                      help="Optional title of the web page")
  parser.add_argument("-o", "--output", dest="output",
                      help="Write html output to this file rather than stdout")
  parser.add_argument("json_file_list", nargs="+", type=CommaSeparatedList,
                      help="[column name,]./path-to/result.json - a comma-separated" +
                      " list of optional column name and paths to json files")

  args = parser.parse_args()
  Render(args)
