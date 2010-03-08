#!/usr/bin/env python
# encoding: utf-8
# Thomas Nagy, 2005 (ita)

import ansiterm
import os, re, logging, traceback, sys
from Constants import *

zones = ''
verbose = 0

colors_lst = {
'USE' : True,
'BOLD'  :'\x1b[01;1m',
'RED'   :'\x1b[01;31m',
'GREEN' :'\x1b[32m',
'YELLOW':'\x1b[33m',
'PINK'  :'\x1b[35m',
'BLUE'  :'\x1b[01;34m',
'CYAN'  :'\x1b[36m',
'NORMAL':'\x1b[0m',
'cursor_on'  :'\x1b[?25h',
'cursor_off' :'\x1b[?25l',
}

got_tty = False
term = os.environ.get('TERM', 'dumb')
if not term in ['dumb', 'emacs']:
	try:
		got_tty = sys.stderr.isatty() or (sys.platform == 'win32' and term in ['xterm', 'msys'])
	except AttributeError:
		pass

import Utils

if not got_tty or 'NOCOLOR' in os.environ:
	colors_lst['USE'] = False

# test
#if sys.platform == 'win32':
#	colors_lst['USE'] = True

def get_color(cl):
	if not colors_lst['USE']: return ''
	return colors_lst.get(cl, '')

class foo(object):
	def __getattr__(self, a):
		return get_color(a)
	def __call__(self, a):
		return get_color(a)

colors = foo()

re_log = re.compile(r'(\w+): (.*)', re.M)
class log_filter(logging.Filter):
	def __init__(self, name=None):
		pass

	def filter(self, rec):
		rec.c1 = colors.PINK
		rec.c2 = colors.NORMAL
		rec.zone = rec.module
		if rec.levelno >= logging.INFO:
			if rec.levelno >= logging.ERROR:
				rec.c1 = colors.RED
			elif rec.levelno >= logging.WARNING:
				rec.c1 = colors.YELLOW
			else:
				rec.c1 = colors.GREEN
			return True

		zone = ''
		m = re_log.match(rec.msg)
		if m:
			zone = rec.zone = m.group(1)
			rec.msg = m.group(2)

		if zones:
			return getattr(rec, 'zone', '') in zones or '*' in zones
		elif not verbose > 2:
			return False
		return True

class formatter(logging.Formatter):
	def __init__(self):
		logging.Formatter.__init__(self, LOG_FORMAT, HOUR_FORMAT)

	def format(self, rec):
		if rec.levelno >= logging.WARNING or rec.levelno == logging.INFO:
			try:
				return '%s%s%s' % (rec.c1, rec.msg.decode('utf-8'), rec.c2)
			except:
				return rec.c1+rec.msg+rec.c2
		return logging.Formatter.format(self, rec)

def debug(*k, **kw):
	if verbose:
		k = list(k)
		k[0] = k[0].replace('\n', ' ')
		logging.debug(*k, **kw)

def error(*k, **kw):
	logging.error(*k, **kw)
	if verbose > 1:
		if isinstance(k[0], Utils.WafError):
			st = k[0].stack
		else:
			st = traceback.extract_stack()
		if st:
			st = st[:-1]
			buf = []
			for filename, lineno, name, line in st:
				buf.append('  File "%s", line %d, in %s' % (filename, lineno, name))
				if line:
					buf.append('	%s' % line.strip())
			if buf: logging.error("\n".join(buf))

warn = logging.warn
info = logging.info

def init_log():
	log = logging.getLogger()
	log.handlers = []
	log.filters = []
	hdlr = logging.StreamHandler()
	hdlr.setFormatter(formatter())
	log.addHandler(hdlr)
	log.addFilter(log_filter())
	log.setLevel(logging.DEBUG)

# may be initialized more than once
init_log()

