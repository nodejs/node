#!/usr/bin/env python
# encoding: utf-8
# ita 2010

import Logs, Utils, Build, Task

def say(txt):
	Logs.warn("^o^: %s" % txt)

try:
	ret = Utils.cmd_output('which cowsay 2> /dev/null').strip()
except Exception, e:
	pass
else:
	def say(txt):
		f = Utils.cmd_output([ret, txt])
		Utils.pprint('PINK', f)

say('you make the errors, we detect them')

def check_task_classes(self):
	for x in Task.TaskBase.classes:
		if isinstance(x, Task.Task):
			if not getattr(cls, 'ext_in', None) or getattr(cls, 'before', None):
				say('class %s has no precedence constraints (ext_in/before)')
			if not getattr(cls, 'ext_out', None) or getattr(cls, 'after', None):
				say('class %s has no precedence constraints (ext_out/after)')

comp = Build.BuildContext.compile
def compile(self):
	if not getattr(self, 'magic', None):
		check_task_classes(self)
	return comp(self)
Build.BuildContext.compile = compile

