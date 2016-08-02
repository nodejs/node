#!/usr/bin/env python
# Copyright 2016 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''
Usage: runtime-call-stats.py [-h] <command> ...

Optional arguments:
  -h, --help  show this help message and exit

Commands:
  run         run chrome with --runtime-call-stats and generate logs
  stats       process logs and print statistics
  json        process logs from several versions and generate JSON
  help        help information

For each command, you can try ./runtime-call-stats.py help command.
'''

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

import numpy
import scipy
import scipy.stats
from math import sqrt


# Run benchmarks.

def print_command(cmd_args):
  def fix_for_printing(arg):
    m = re.match(r'^--([^=]+)=(.*)$', arg)
    if m and (' ' in m.group(2) or m.group(2).startswith('-')):
      arg = "--{}='{}'".format(m.group(1), m.group(2))
    elif ' ' in arg:
      arg = "'{}'".format(arg)
    return arg
  print " ".join(map(fix_for_printing, cmd_args))


def start_replay_server(args, sites):
  with tempfile.NamedTemporaryFile(prefix='callstats-inject-', suffix='.js',
                                   mode='wt', delete=False) as f:
    injection = f.name
    generate_injection(f, sites, args.refresh)
  cmd_args = [
      args.replay_bin,
      "--port=4080",
      "--ssl_port=4443",
      "--no-dns_forwarding",
      "--use_closest_match",
      "--no-diff_unknown_requests",
      "--inject_scripts=deterministic.js,{}".format(injection),
      args.replay_wpr,
  ]
  print "=" * 80
  print_command(cmd_args)
  with open(os.devnull, 'w') as null:
    server = subprocess.Popen(cmd_args, stdout=null, stderr=null)
  print "RUNNING REPLAY SERVER: %s with PID=%s" % (args.replay_bin, server.pid)
  print "=" * 80
  return {'process': server, 'injection': injection}


def stop_replay_server(server):
  print("SHUTTING DOWN REPLAY SERVER %s" % server['process'].pid)
  server['process'].terminate()
  os.remove(server['injection'])


def generate_injection(f, sites, refreshes=0):
  print >> f, """\
(function() {
  let s = window.sessionStorage.getItem("refreshCounter");
  let refreshTotal = """, refreshes, """;
  let refreshCounter = s ? parseInt(s) : refreshTotal;
  let refreshId = refreshTotal - refreshCounter;
  if (refreshCounter > 0) {
    window.sessionStorage.setItem("refreshCounter", refreshCounter-1);
  }

  function match(url, item) {
    if ('regexp' in item) return url.match(item.regexp) !== null;
    let url_wanted = item.url;
    // Allow automatic redirections from http to https.
    if (url_wanted.startsWith("http://") && url.startsWith("https://")) {
      url_wanted = "https://" + url_wanted.substr(7);
    }
    return url.startsWith(url_wanted);
  };

  function onLoad(e) {
    let url = e.target.URL;
    for (let item of sites) {
      if (!match(url, item)) continue;
      let timeout = 'timeline' in item ? 2500 * item.timeline
                  : 'timeout'  in item ? 1000 * (item.timeout - 3)
                  : 10000;
      console.log("Setting time out of " + timeout + " for: " + url);
      window.setTimeout(function() {
        console.log("Time is out for: " + url);
        let msg = "STATS: (" + refreshId + ") " + url;
        %GetAndResetRuntimeCallStats(1, msg);
        if (refreshCounter > 0) {
          console.log("Refresh counter is " + refreshCounter + ", refreshing: " + url);
          window.location.reload();
        }
      }, timeout);
      return;
    }
    console.log("Ignoring: " + url);
  };

  let sites =
    """, json.dumps(sites), """;

  console.log("Event listenner added for: " + window.location.href);
  window.addEventListener("load", onLoad);
})();"""


def run_site(site, domain, args, timeout=None):
  print "="*80
  print "RUNNING DOMAIN %s" % domain
  print "="*80
  result_template = "{domain}#{count}.txt" if args.repeat else "{domain}.txt"
  count = 0
  if timeout is None: timeout = args.timeout
  if args.replay_wpr:
    timeout *= 1 + args.refresh
    timeout += 1
  while count == 0 or args.repeat is not None and count < args.repeat:
    count += 1
    result = result_template.format(domain=domain, count=count)
    retries = 0
    while args.retries is None or retries < args.retries:
      retries += 1
      try:
        if args.user_data_dir:
          user_data_dir = args.user_data_dir
        else:
          user_data_dir = tempfile.mkdtemp(prefix="chr_")
        js_flags = "--runtime-call-stats"
        if args.replay_wpr: js_flags += " --allow-natives-syntax"
        if args.js_flags: js_flags += " " + args.js_flags
        chrome_flags = [
            "--no-default-browser-check",
            "--disable-translate",
            "--js-flags={}".format(js_flags),
            "--no-first-run",
            "--user-data-dir={}".format(user_data_dir),
        ]
        if args.replay_wpr:
          chrome_flags += [
              "--host-resolver-rules=MAP *:80 localhost:4080, "  \
                                    "MAP *:443 localhost:4443, " \
                                    "EXCLUDE localhost",
              "--ignore-certificate-errors",
              "--disable-seccomp-sandbox",
              "--disable-web-security",
              "--reduce-security-for-testing",
              "--allow-insecure-localhost",
          ]
        else:
          chrome_flags += [
              "--single-process",
          ]
        if args.chrome_flags:
          chrome_flags += args.chrome_flags.split()
        cmd_args = [
            "timeout", str(timeout),
            args.with_chrome
        ] + chrome_flags + [ site ]
        print "- " * 40
        print_command(cmd_args)
        print "- " * 40
        with open(result, "wt") as f:
          status = subprocess.call(cmd_args, stdout=f)
        # 124 means timeout killed chrome, 0 means the user was bored first!
        # If none of these two happened, then chrome apparently crashed, so
        # it must be called again.
        if status != 124 and status != 0:
          print("CHROME CRASHED, REPEATING RUN");
          continue
        # If the stats file is empty, chrome must be called again.
        if os.path.isfile(result) and os.path.getsize(result) > 0:
          if args.print_url:
            with open(result, "at") as f:
              print >> f
              print >> f, "URL: {}".format(site)
          break
        if retries <= 6: timeout += 2 ** (retries-1)
        print("EMPTY RESULT, REPEATING RUN");
      finally:
        if not args.user_data_dir:
          shutil.rmtree(user_data_dir)


def read_sites_file(args):
  try:
    sites = []
    try:
      with open(args.sites_file, "rt") as f:
        for item in json.load(f):
          if 'timeout' not in item:
            # This is more-or-less arbitrary.
            item['timeout'] = int(2.5 * item['timeline'] + 3)
          if item['timeout'] > args.timeout: item['timeout'] = args.timeout
          sites.append(item)
    except ValueError:
      with open(args.sites_file, "rt") as f:
        for line in f:
          line = line.strip()
          if not line or line.startswith('#'): continue
          sites.append({'url': line, 'timeout': args.timeout})
    return sites
  except IOError as e:
    args.error("Cannot read from {}. {}.".format(args.sites_file, e.strerror))
    sys.exit(1)


def do_run(args):
  # Determine the websites to benchmark.
  if args.sites_file:
    sites = read_sites_file(args)
  else:
    sites = [{'url': site, 'timeout': args.timeout} for site in args.sites]
  # Disambiguate domains, if needed.
  L = []
  domains = {}
  for item in sites:
    site = item['url']
    m = re.match(r'^(https?://)?([^/]+)(/.*)?$', site)
    if not m:
      args.error("Invalid URL {}.".format(site))
      continue
    domain = m.group(2)
    entry = [site, domain, None, item['timeout']]
    if domain not in domains:
      domains[domain] = entry
    else:
      if not isinstance(domains[domain], int):
        domains[domain][2] = 1
        domains[domain] = 1
      domains[domain] += 1
      entry[2] = domains[domain]
    L.append(entry)
  replay_server = start_replay_server(args, sites) if args.replay_wpr else None
  try:
    # Run them.
    for site, domain, count, timeout in L:
      if count is not None: domain = "{}%{}".format(domain, count)
      print site, domain, timeout
      run_site(site, domain, args, timeout)
  finally:
    if replay_server:
      stop_replay_server(replay_server)


# Calculate statistics.

def statistics(data):
  N = len(data)
  average = numpy.average(data)
  median = numpy.median(data)
  low = numpy.min(data)
  high= numpy.max(data)
  if N > 1:
    # evaluate sample variance by setting delta degrees of freedom (ddof) to
    # 1. The degree used in calculations is N - ddof
    stddev = numpy.std(data, ddof=1)
    # Get the endpoints of the range that contains 95% of the distribution
    t_bounds = scipy.stats.t.interval(0.95, N-1)
    #assert abs(t_bounds[0] + t_bounds[1]) < 1e-6
    # sum mean to the confidence interval
    ci = {
        'abs': t_bounds[1] * stddev / sqrt(N),
        'low': average + t_bounds[0] * stddev / sqrt(N),
        'high': average + t_bounds[1] * stddev / sqrt(N)
    }
  else:
    stddev = 0
    ci = { 'abs': 0, 'low': average, 'high': average }
  if abs(stddev) > 0.0001 and abs(average) > 0.0001:
    ci['perc'] = t_bounds[1] * stddev / sqrt(N) / average * 100
  else:
    ci['perc'] = 0
  return { 'samples': N, 'average': average, 'median': median,
           'stddev': stddev, 'min': low, 'max': high, 'ci': ci }


def read_stats(path, S):
  with open(path, "rt") as f:
    # Process the whole file and sum repeating entries.
    D = { 'Sum': {'time': 0, 'count': 0} }
    for line in f:
      line = line.strip()
      # Discard headers and footers.
      if not line: continue
      if line.startswith("Runtime Function"): continue
      if line.startswith("===="): continue
      if line.startswith("----"): continue
      if line.startswith("URL:"): continue
      if line.startswith("STATS:"): continue
      # We have a regular line.
      fields = line.split()
      key = fields[0]
      time = float(fields[1].replace("ms", ""))
      count = int(fields[3])
      if key not in D: D[key] = { 'time': 0, 'count': 0 }
      D[key]['time'] += time
      D[key]['count'] += count
      # We calculate the sum, if it's not the "total" line.
      if key != "Total":
        D['Sum']['time'] += time
        D['Sum']['count'] += count
    # Append the sums as single entries to S.
    for key in D:
      if key not in S: S[key] = { 'time_list': [], 'count_list': [] }
      S[key]['time_list'].append(D[key]['time'])
      S[key]['count_list'].append(D[key]['count'])


def print_stats(S, args):
  # Sort by ascending/descending time average, then by ascending/descending
  # count average, then by ascending name.
  def sort_asc_func(item):
    return (item[1]['time_stat']['average'],
            item[1]['count_stat']['average'],
            item[0])
  def sort_desc_func(item):
    return (-item[1]['time_stat']['average'],
            -item[1]['count_stat']['average'],
            item[0])
  # Sorting order is in the commend-line arguments.
  sort_func = sort_asc_func if args.sort == "asc" else sort_desc_func
  # Possibly limit how many elements to print.
  L = [item for item in sorted(S.items(), key=sort_func)
       if item[0] not in ["Total", "Sum"]]
  N = len(L)
  if args.limit == 0:
    low, high = 0, N
  elif args.sort == "desc":
    low, high = 0, args.limit
  else:
    low, high = N-args.limit, N
  # How to print entries.
  def print_entry(key, value):
    def stats(s, units=""):
      conf = "{:0.1f}({:0.2f}%)".format(s['ci']['abs'], s['ci']['perc'])
      return "{:8.1f}{} +/- {:15s}".format(s['average'], units, conf)
    print "{:>50s}  {}  {}".format(
      key,
      stats(value['time_stat'], units="ms"),
      stats(value['count_stat'])
    )
  # Print and calculate partial sums, if necessary.
  for i in range(low, high):
    print_entry(*L[i])
    if args.totals and args.limit != 0:
      if i == low:
        partial = { 'time_list': [0] * len(L[i][1]['time_list']),
                    'count_list': [0] * len(L[i][1]['count_list']) }
      assert len(partial['time_list']) == len(L[i][1]['time_list'])
      assert len(partial['count_list']) == len(L[i][1]['count_list'])
      for j, v in enumerate(L[i][1]['time_list']):
        partial['time_list'][j] += v
      for j, v in enumerate(L[i][1]['count_list']):
        partial['count_list'][j] += v
  # Print totals, if necessary.
  if args.totals:
    print '-' * 80
    if args.limit != 0:
      partial['time_stat'] = statistics(partial['time_list'])
      partial['count_stat'] = statistics(partial['count_list'])
      print_entry("Partial", partial)
    print_entry("Sum", S["Sum"])
    print_entry("Total", S["Total"])


def do_stats(args):
  T = {}
  for path in args.logfiles:
    filename = os.path.basename(path)
    m = re.match(r'^([^#]+)(#.*)?$', filename)
    domain = m.group(1)
    if domain not in T: T[domain] = {}
    read_stats(path, T[domain])
  for i, domain in enumerate(sorted(T)):
    if len(T) > 1:
      if i > 0: print
      print "{}:".format(domain)
      print '=' * 80
    S = T[domain]
    for key in S:
      S[key]['time_stat'] = statistics(S[key]['time_list'])
      S[key]['count_stat'] = statistics(S[key]['count_list'])
    print_stats(S, args)


# Generate JSON file.

def do_json(args):
  J = {}
  for path in args.logdirs:
    if os.path.isdir(path):
      for root, dirs, files in os.walk(path):
        version = os.path.basename(root)
        if version not in J: J[version] = {}
        for filename in files:
          if filename.endswith(".txt"):
            m = re.match(r'^([^#]+)(#.*)?\.txt$', filename)
            domain = m.group(1)
            if domain not in J[version]: J[version][domain] = {}
            read_stats(os.path.join(root, filename), J[version][domain])
  for version, T in J.items():
    for domain, S in T.items():
      A = []
      for name, value in S.items():
        # We don't want the calculated sum in the JSON file.
        if name == "Sum": continue
        entry = [name]
        for x in ['time_list', 'count_list']:
          s = statistics(S[name][x])
          entry.append(round(s['average'], 1))
          entry.append(round(s['ci']['abs'], 1))
          entry.append(round(s['ci']['perc'], 2))
        A.append(entry)
      T[domain] = A
  print json.dumps(J, separators=(',', ':'))


# Help.

def do_help(parser, subparsers, args):
  if args.help_cmd:
    if args.help_cmd in subparsers:
      subparsers[args.help_cmd].print_help()
    else:
      args.error("Unknown command '{}'".format(args.help_cmd))
  else:
    parser.print_help()


# Main program, parse command line and execute.

def coexist(*l):
  given = sum(1 for x in l if x)
  return given == 0 or given == len(l)

def main():
  parser = argparse.ArgumentParser()
  subparser_adder = parser.add_subparsers(title="commands", dest="command",
                                          metavar="<command>")
  subparsers = {}
  # Command: run.
  subparsers["run"] = subparser_adder.add_parser(
      "run", help="run --help")
  subparsers["run"].set_defaults(
      func=do_run, error=subparsers["run"].error)
  subparsers["run"].add_argument(
      "--chrome-flags", type=str, default="",
      help="specify additional chrome flags")
  subparsers["run"].add_argument(
      "--js-flags", type=str, default="",
      help="specify additional V8 flags")
  subparsers["run"].add_argument(
      "--no-url", dest="print_url", action="store_false", default=True,
      help="do not include url in statistics file")
  subparsers["run"].add_argument(
      "-n", "--repeat", type=int, metavar="<num>",
      help="specify iterations for each website (default: once)")
  subparsers["run"].add_argument(
      "-k", "--refresh", type=int, metavar="<num>", default=0,
      help="specify refreshes for each iteration (default: 0)")
  subparsers["run"].add_argument(
      "--replay-wpr", type=str, metavar="<path>",
      help="use the specified web page replay (.wpr) archive")
  subparsers["run"].add_argument(
      "--replay-bin", type=str, metavar="<path>",
      help="specify the replay.py script typically located in " \
           "$CHROMIUM/src/third_party/webpagereplay/replay.py")
  subparsers["run"].add_argument(
      "-r", "--retries", type=int, metavar="<num>",
      help="specify retries if website is down (default: forever)")
  subparsers["run"].add_argument(
      "-f", "--sites-file", type=str, metavar="<path>",
      help="specify file containing benchmark websites")
  subparsers["run"].add_argument(
      "-t", "--timeout", type=int, metavar="<seconds>", default=60,
      help="specify seconds before chrome is killed")
  subparsers["run"].add_argument(
      "-u", "--user-data-dir", type=str, metavar="<path>",
      help="specify user data dir (default is temporary)")
  subparsers["run"].add_argument(
      "-c", "--with-chrome", type=str, metavar="<path>",
      default="/usr/bin/google-chrome",
      help="specify chrome executable to use")
  subparsers["run"].add_argument(
      "sites", type=str, metavar="<URL>", nargs="*",
      help="specify benchmark website")
  # Command: stats.
  subparsers["stats"] = subparser_adder.add_parser(
      "stats", help="stats --help")
  subparsers["stats"].set_defaults(
      func=do_stats, error=subparsers["stats"].error)
  subparsers["stats"].add_argument(
      "-l", "--limit", type=int, metavar="<num>", default=0,
      help="limit how many items to print (default: none)")
  subparsers["stats"].add_argument(
      "-s", "--sort", choices=["asc", "desc"], default="asc",
      help="specify sorting order (default: ascending)")
  subparsers["stats"].add_argument(
      "-n", "--no-total", dest="totals", action="store_false", default=True,
      help="do not print totals")
  subparsers["stats"].add_argument(
      "logfiles", type=str, metavar="<logfile>", nargs="*",
      help="specify log files to parse")
  # Command: json.
  subparsers["json"] = subparser_adder.add_parser(
      "json", help="json --help")
  subparsers["json"].set_defaults(
      func=do_json, error=subparsers["json"].error)
  subparsers["json"].add_argument(
      "logdirs", type=str, metavar="<logdir>", nargs="*",
      help="specify directories with log files to parse")
  # Command: help.
  subparsers["help"] = subparser_adder.add_parser(
      "help", help="help information")
  subparsers["help"].set_defaults(
      func=lambda args: do_help(parser, subparsers, args),
      error=subparsers["help"].error)
  subparsers["help"].add_argument(
      "help_cmd", type=str, metavar="<command>", nargs="?",
      help="command for which to display help")
  # Execute the command.
  args = parser.parse_args()
  setattr(args, 'script_path', os.path.dirname(sys.argv[0]))
  if args.command == "run" and coexist(args.sites_file, args.sites):
    args.error("use either option --sites-file or site URLs")
    sys.exit(1)
  elif args.command == "run" and not coexist(args.replay_wpr, args.replay_bin):
    args.error("options --replay-wpr and --replay-bin must be used together")
    sys.exit(1)
  else:
    args.func(args)

if __name__ == "__main__":
  sys.exit(main())
