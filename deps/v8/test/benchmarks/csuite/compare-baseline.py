#!/usr/bin/python3
# Copyright 2018 the V8 project authors. All rights reserved.

'''
python %prog [options] [baseline_files]

Compare benchmark results from the benchmark runner against one or
more baselines. You can either pipe the result of the benchmark
runner directly into this script or specify the results file with
the -f option.
'''

# for py2/py3 compatibility
from __future__ import print_function

import csv
import math
from optparse import OptionParser
import os
import sys

PERCENT_CONSIDERED_SIGNIFICANT = 0.5
PROBABILITY_CONSIDERED_SIGNIFICANT = 0.02
PROBABILITY_CONSIDERED_MEANINGLESS = 0.05

RESET_SEQ = "\033[0m"
RED_SEQ = "\033[31m"
GREEN_SEQ = "\033[32m"
BLUE_SEQ = "\033[34m"
BOLD_SEQ = "\033[1m"

v8_benchmarks = ["V8", "Octane", "Richards", "DeltaBlue", "Crypto",
                 "EarleyBoyer", "RayTrace", "RegExp", "Splay", "SplayLatency",
                 "NavierStokes", "PdfJS", "Mandreel", "MandreelLatency",
                 "Gameboy", "CodeLoad", "Box2D", "zlib", "Typescript"]

suite_names = ["V8", "Octane", "Kraken-Orig", "Kraken-Once", "Kraken",
               "SunSpider", "SunSpider-Once", "SunSpider-Orig"]

def ColorText(opts, text):
  if opts.no_color:
    result = text.replace("$RESET", "")
    result = result.replace("$BLUE", "")
    result = result.replace("$RED", "")
    result = result.replace("$GREEN", "")
    result = result.replace("$BOLD", "")
  else:
    if opts.html:
      result = text.replace("$RESET", "</font></b>")
      result = result.replace("$BLUE", "<font COLOR=\"0000DD\">")
      result = result.replace("$RED", "<font COLOR=\"DD0000\">")
      result = result.replace("$GREEN", "<font COLOR=\"00DD00\">")
      result = result.replace("$BOLD", "<b>")
    else:
      result = text.replace("$RESET", RESET_SEQ)
      result = result.replace("$BLUE", BLUE_SEQ)
      result = result.replace("$RED", RED_SEQ)
      result = result.replace("$GREEN", GREEN_SEQ)
      result = result.replace("$BOLD", BOLD_SEQ)
  return result

def NormalizedSigmaToString(normalized_sigma):
  assert normalized_sigma >= 0
  if normalized_sigma < PROBABILITY_CONSIDERED_SIGNIFICANT:
    return "|"
  return "S"

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

def PercentColor(change_percent, flakyness):
  result = ""
  if change_percent >= PERCENT_CONSIDERED_SIGNIFICANT:
    result = "$GREEN"
  elif change_percent <= -PERCENT_CONSIDERED_SIGNIFICANT:
    result = "$RED"
  else:
    return ""
  if flakyness < PROBABILITY_CONSIDERED_SIGNIFICANT:
    result += "$BOLD"
  elif flakyness > PROBABILITY_CONSIDERED_MEANINGLESS:
    result = ""
  return result

def ProcessOneResultLine(opts, suite, testname, time, sigma, num, baselines):
  time = float(time)
  sigma = float(sigma)
  num = int(num)
  if testname in suite_names:
    base_color = "$BOLD"
  else:
    base_color = ""
  if opts.html:
    line_out = ("<tr><td>%s%s$RESET</td><td>%s%8.1f$RESET</td>" %
                (base_color, testname, base_color, time))
  else:
    sigma_string = NormalizedSigmaToString(sigma / time)
    line_out = ("%s%40s$RESET: %s%8.1f$RESET %s" %
                (base_color, testname, base_color, time, sigma_string))
  for baseline in baselines:
    raw_score = ""
    compare_score = ""
    found = False
    if suite in baseline[1]:
      baseline_results = baseline[1][suite]
      for item in baseline_results:
        if testname == item[0]:
          found = True
          raw_score_num = float(item[1])
          raw_sigma_num = float(item[2])
          raw_score = "%7.1f" % raw_score_num
          compare_num = 0
          compare_score = ""
          percent_color = ""
          if testname in v8_benchmarks:
            compare_num = 100*time/raw_score_num - 100
          else:
            compare_num = 100*raw_score_num/time - 100
          if abs(compare_num) > 0.1:
            compare_score = "%3.1f" % (compare_num)
            z = ComputeZ(raw_score_num, raw_sigma_num, time, num)
            p = ComputeProbability(z)
            percent_color = PercentColor(compare_num, p)
          sigma_string = NormalizedSigmaToString(raw_sigma_num / raw_score_num)
          if opts.html:
            format_string = "<td>%s%8s$RESET</td><td>%s%6s$RESET</td>"
          else:
            format_string = " %s%8s$RESET %s %s%6s$RESET |"
          line_out += (format_string %
              (base_color, raw_score, sigma_string,
               percent_color, compare_score))
    if not found:
      if opts.html:
        line_out += "<td></td><td></td>"
      else:
        line_out += "|          |        "
  if opts.html:
    line_out += "</tr>"
  print(ColorText(opts, line_out))

def PrintSeparator(opts, baselines, big):
  if not opts.html:
    if big:
      separator = "==================================================="
    else:
      separator = "---------------------------------------------------"
    for baseline in baselines:
      if big:
        separator += "+==========+========"
      else:
        separator += "+----------+--------"
    separator += "+"
    print(separator)

def ProcessResults(opts, results, baselines):
  for suite in suite_names:
    if suite in results:
      for result in results[suite]:
        ProcessOneResultLine(opts, suite, result[0], result[1], result[2],
                             result[3], baselines);
      PrintSeparator(opts, baselines, False)

def ProcessFile(file_path):
  file_reader = csv.reader(open(file_path, 'r'), delimiter=',')
  benchmark_results = {}
  current_rows = []
  for row in file_reader:
    if len(row) > 1:
      current_rows.append(row)
      for suite in suite_names:
        if row[0] == suite:
          benchmark_results[row[0]] = current_rows
          current_rows = []
  return benchmark_results

def ProcessStdIn():
  benchmark_results = {}
  current_rows = []
  for line_in in sys.stdin:
    line_in = line_in.rstrip()
    row = line_in.split(",")
    if len(row) > 1:
      current_rows.append(row)
      for suite in suite_names:
        if row[0] == suite:
          benchmark_results[row[0]] = current_rows
          current_rows = []
  return benchmark_results

def CompareFiles(opts, args):
  results = []
  baselines = []
  for file_path in args:
    baseline = ProcessFile(file_path)
    baselines.append((os.path.basename(file_path), baseline))
  if opts.html:
    header = "<tr><th>benchmark</th><th>score</th>"
  else:
    header = "%40s: %8s " % ("benchmark", "score")
  for baseline in baselines:
    (baseline_name, baseline_results) = baseline
    if opts.html:
      header += ("<th>%s</th><th>%s</th>") % (baseline_name[0:7], "%")
    else:
      header += "| %8s | %6s " % (baseline_name[0:7], "%")
  if opts.html:
    header += "</tr>\n"
  else:
    header += "|"
  print(header)
  PrintSeparator(opts, baselines, True)
  if opts.filename:
    file_reader = csv.reader(open(opts.filename, 'rb'), delimiter=',')
    results = ProcessFile(opts.filename)
  else:
    results = ProcessStdIn()
  ProcessResults(opts, results, baselines)

if __name__ == '__main__':
  parser = OptionParser(usage=__doc__)
  parser.add_option("-f", "--filename", dest="filename",
                    help="Specifies the filename for the results to "\
"compare to the baselines rather than reading from stdin.")
  parser.add_option("-b", "--baselines", dest="baselines",
                    help="Specifies a directory of baseline files to "\
"compare against.")
  parser.add_option("-n", "--no-color", action="store_true",
                    dest="no_color", default=False,
                    help="Generates output without escape codes that "\
"add color highlights.")
  parser.add_option("--html", action="store_true",
                    dest="html", default=False,
                    help="Generates output as a HTML table ")
  (opts, args) = parser.parse_args()
  if opts.baselines:
    args.extend(map(lambda x: (opts.baselines + "/" + x),
                    (os.listdir(opts.baselines))))
  args = reversed(sorted(args))
  CompareFiles(opts, args)
