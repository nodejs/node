#!/usr/bin/python
# Copyright 2018 the V8 project authors. All rights reserved.

'''
python %prog -c <command> [options]

Local benchmark runner.
The -c option is mandatory.
'''

import math
from optparse import OptionParser
import os
import re
import subprocess
import sys
import time

def GeometricMean(numbers):
  log = sum([math.log(n) for n in numbers])
  return math.pow(math.e, log / len(numbers))


class BenchmarkSuite(object):

  def __init__(self, name):
    self.name = name
    self.results = {}
    self.tests = []
    self.avgresult = {}
    self.sigmaresult = {}
    self.numresult = {}
    self.kClassicScoreSuites = ["SunSpider", "Kraken"]
    self.kGeometricScoreSuites = ["Octane"]


  def RecordResult(self, test, result):
    if test not in self.tests:
      self.tests += [test]
      self.results[test] = []
    self.results[test] += [int(result)]

  def ThrowAwayWorstResult(self, results):
    if len(results) <= 1: return
    if self.name in self.kClassicScoreSuites:
      results.pop()
    elif self.name in self.kGeometricScoreSuites:
      del results[0]

  def ProcessResults(self, opts):
    for test in self.tests:
      results = self.results[test]
      results.sort()
      self.ThrowAwayWorstResult(results)
      mean = sum(results) * 1.0 / len(results)
      self.avgresult[test] = mean
      sigma_divisor = len(results) - 1
      if sigma_divisor == 0:
        sigma_divisor = 1
      self.sigmaresult[test] = math.sqrt(
          sum((x - mean) ** 2 for x in results) / sigma_divisor)
      self.numresult[test] = len(results)
      if opts.verbose:
        if not test in ["Octane"]:
          print("%s,%.1f,%.2f,%d" %
              (test, self.avgresult[test],
               self.sigmaresult[test], self.numresult[test]))

  def ComputeScoreGeneric(self):
    self.score = 0
    self.sigma = 0
    for test in self.tests:
      self.score += self.avgresult[test]
      self.sigma += self.sigmaresult[test]
      self.num = self.numresult[test]

  def ComputeScoreV8Octane(self, name):
    # The score for the run is stored with the form
    # "Octane-octane2.1(Score): <score>"
    found_name = ''
    for s in self.avgresult.keys():
      if re.search("^Octane", s):
        found_name = s
        break

    self.score = self.avgresult[found_name]
    self.sigma = 0
    for test in self.tests:
      self.sigma += self.sigmaresult[test]
      self.num = self.numresult[test]
    self.sigma /= len(self.tests)

  def ComputeScore(self):
    if self.name in self.kClassicScoreSuites:
      self.ComputeScoreGeneric()
    elif self.name in self.kGeometricScoreSuites:
      self.ComputeScoreV8Octane(self.name)
    else:
      print "Don't know how to compute score for suite: '%s'" % self.name

  def IsBetterThan(self, other):
    if self.name in self.kClassicScoreSuites:
      return self.score < other.score
    elif self.name in self.kGeometricScoreSuites:
      return self.score > other.score
    else:
      print "Don't know how to compare score for suite: '%s'" % self.name


class BenchmarkRunner(object):
  def __init__(self, args, current_directory, opts):
    self.best = {}
    self.second_best = {}
    self.args = args
    self.opts = opts
    self.current_directory = current_directory
    self.outdir = os.path.join(opts.cachedir, "_benchmark_runner_data")

  def Run(self):
    if not os.path.exists(self.outdir):
      os.mkdir(self.outdir)

    self.RunCommand()
    # Figure out the suite from the command line (heuristic) or the current
    # working directory.
    teststr = opts.command.lower() + " " + self.current_directory.lower()
    if teststr.find('octane') >= 0:
      suite = 'Octane'
    elif teststr.find('sunspider') >= 0:
      suite = 'SunSpider'
    elif teststr.find('kraken') >= 0:
      suite = 'Kraken'
    else:
      suite = 'Generic'

    self.ProcessOutput(suite)

  def RunCommand(self):
    for i in range(self.opts.runs):
      outfile = "%s/out.%d.txt" % (self.outdir, i)
      if os.path.exists(outfile) and not self.opts.force:
        continue
      print "run #%d" % i
      cmdline = "%s > %s" % (self.opts.command, outfile)
      subprocess.call(cmdline, shell=True)
      time.sleep(self.opts.sleep)

  def ProcessLine(self, line):
    # Octane puts this line in before score.
    if line == "----":
      return (None, None)

    # Kraken or Sunspider?
    g = re.match("(?P<test_name>\w+(-\w+)*)\(RunTime\): (?P<score>\d+) ms\.", \
        line)
    if g == None:
      # Octane?
      g = re.match("(?P<test_name>\w+): (?P<score>\d+)", line)
      if g == None:
        g = re.match("Score \(version [0-9]+\): (?P<score>\d+)", line)
        if g != None:
          return ('Octane', g.group('score'))
        else:
          # Generic?
          g = re.match("(?P<test_name>\w+)\W+(?P<score>\d+)", line)
          if g == None:
            return (None, None)
    return (g.group('test_name'), g.group('score'))

  def ProcessOutput(self, suitename):
    suite = BenchmarkSuite(suitename)
    for i in range(self.opts.runs):
      outfile = "%s/out.%d.txt" % (self.outdir, i)
      with open(outfile, 'r') as f:
        for line in f:
          (test, result) = self.ProcessLine(line)
          if test != None:
            suite.RecordResult(test, result)

    suite.ProcessResults(self.opts)
    suite.ComputeScore()
    print ("%s,%.1f,%.2f,%d " %
        (suite.name, suite.score, suite.sigma, suite.num)),
    if self.opts.verbose:
      print ""
    print ""


if __name__ == '__main__':
  parser = OptionParser(usage=__doc__)
  parser.add_option("-c", "--command", dest="command",
                    help="Command to run the test suite.")
  parser.add_option("-r", "--runs", dest="runs", default=4,
                    help="Number of runs")
  parser.add_option("-v", "--verbose", dest="verbose", action="store_true",
                    default=False, help="Print results for each test")
  parser.add_option("-f", "--force", dest="force", action="store_true",
                    default=False,
                    help="Force re-run even if output files exist")
  parser.add_option("-z", "--sleep", dest="sleep", default=0,
                    help="Number of seconds to sleep between runs")
  parser.add_option("-d", "--run-directory", dest="cachedir",
                    help="Directory where a cache directory will be created")
  (opts, args) = parser.parse_args()
  opts.runs = int(opts.runs)
  opts.sleep = int(opts.sleep)

  if not opts.command:
    print "You must specify the command to run (-c). Aborting."
    sys.exit(1)

  cachedir = os.path.abspath(os.getcwd())
  if not opts.cachedir:
    opts.cachedir = cachedir
  if not os.path.exists(opts.cachedir):
    print "Directory " + opts.cachedir + " is not valid. Aborting."
    sys.exit(1)

  br = BenchmarkRunner(args, os.getcwd(), opts)
  br.Run()
