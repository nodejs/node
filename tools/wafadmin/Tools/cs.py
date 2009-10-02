#!/usr/bin/env python
# encoding: utf-8
# Thomas Nagy, 2006 (ita)

"C# support"

import TaskGen, Utils, Task
from Logs import error
from TaskGen import before, after, taskgen, feature

flag_vars= ['FLAGS', 'ASSEMBLIES']

@feature('cs')
def init_cs(self):
	Utils.def_attrs(self,
		flags = '',
		assemblies = '',
		resources = '',
		uselib = '')

@feature('cs')
@after('init_cs')
def apply_uselib_cs(self):
	if not self.uselib:
		return
	global flag_vars
	for var in self.to_list(self.uselib):
		for v in self.flag_vars:
			val = self.env[v+'_'+var]
			if val: self.env.append_value(v, val)

@feature('cs')
@after('apply_uselib_cs')
@before('apply_core')
def apply_cs(self):
	try: self.meths.remove('apply_core')
	except ValueError: pass

	# process the flags for the assemblies
	assemblies_flags = []
	for i in self.to_list(self.assemblies) + self.env['ASSEMBLIES']:
		assemblies_flags += '/r:'+i
	self.env['_ASSEMBLIES'] += assemblies_flags

	# process the flags for the resources
	for i in self.to_list(self.resources):
		self.env['_RESOURCES'].append('/resource:'+i)

	# additional flags
	self.env['_FLAGS'] += self.to_list(self.flags) + self.env['FLAGS']

	curnode = self.path

	# process the sources
	nodes = []
	for i in self.to_list(self.source):
		nodes.append(curnode.find_resource(i))

	# create the task
	task = self.create_task('mcs')
	task.inputs  = nodes
	task.set_outputs(self.path.find_or_declare(self.target))

Task.simple_task_type('mcs', '${MCS} ${SRC} /out:${TGT} ${_FLAGS} ${_ASSEMBLIES} ${_RESOURCES}', color='YELLOW')

def detect(conf):
	mcs = conf.find_program('mcs', var='MCS')
	if not mcs: mcs = conf.find_program('gmcs', var='MCS')

