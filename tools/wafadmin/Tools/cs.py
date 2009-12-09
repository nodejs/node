#!/usr/bin/env python
# encoding: utf-8
# Thomas Nagy, 2006 (ita)

"C# support"

import TaskGen, Utils, Task, Options
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
	for i in self.to_list(self.assemblies) + self.env['ASSEMBLIES']:
		self.env.append_unique('_ASSEMBLIES', '/r:'+i)

	# process the flags for the resources
	for i in self.to_list(self.resources):
		self.env.append_unique('_RESOURCES', '/resource:'+i)

	# what kind of assembly are we generating?
	self.env['_TYPE'] = getattr(self, 'type', 'exe')

	# additional flags
	self.env.append_unique('_FLAGS', self.to_list(self.flags))
	self.env.append_unique('_FLAGS', self.env.FLAGS)

	# process the sources
	nodes = [self.path.find_resource(i) for i in self.to_list(self.source)]
	self.create_task('mcs', nodes, self.path.find_or_declare(self.target))

Task.simple_task_type('mcs', '${MCS} ${SRC} /target:${_TYPE} /out:${TGT} ${_FLAGS} ${_ASSEMBLIES} ${_RESOURCES}', color='YELLOW')

def detect(conf):
	csc = getattr(Options.options, 'cscbinary', None)
	if csc:
		conf.env.MCS = csc
	conf.find_program(['gmcs', 'mcs'], var='MCS')

def set_options(opt):
	opt.add_option('--with-csc-binary', type='string', dest='cscbinary')

