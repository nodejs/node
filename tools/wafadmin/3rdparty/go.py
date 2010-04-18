#!/usr/bin/env python
# encoding: utf-8
# go.py - Waf tool for the Go programming language
# By: Tom Wambold <tom5760@gmail.com>

import platform

import Task
import Utils
from TaskGen import feature, extension, after

Task.simple_task_type('gocompile', '${GOC} ${GOCFLAGS} -o ${TGT} ${SRC}', shell=False)
Task.simple_task_type('gopack', '${GOP} grc ${TGT} ${SRC}', shell=False)
Task.simple_task_type('golink', '${GOL} ${GOLFLAGS} -o ${TGT} ${SRC}', shell=False)

def detect(conf):

	def set_def(var, val):
		if not conf.env[var]:
			conf.env[var] = val

	set_def('GO_PLATFORM', platform.machine())

	if conf.env.GO_PLATFORM == 'x86_64':
		set_def('GO_COMPILER', '6g')
		set_def('GO_LINKER', '6l')
		set_def('GO_EXTENSION', '.6')
	elif conf.env.GO_PLATFORM == 'i386':
		set_def('GO_COMPILER', '8g')
		set_def('GO_LINKER', '8l')
		set_def('GO_EXTENSION', '.8')

	if not (conf.env.GO_COMPILER or conf.env.GO_LINKER or conf.env.GO_EXTENSION):
		raise conf.fatal('Unsupported platform ' + platform.machine())

	set_def('GO_PACK', 'gopack')
	set_def('GO_PACK_EXTENSION', '.a')

	conf.find_program(conf.env.GO_COMPILER, var='GOC', mandatory=True)
	conf.find_program(conf.env.GO_LINKER,   var='GOL', mandatory=True)
	conf.find_program(conf.env.GO_PACK,     var='GOP', mandatory=True)

@extension('.go')
def compile_go(self, node):
	try:
		self.go_nodes.append(node)
	except AttributeError:
		self.go_nodes = [node]

@feature('go')
@after('apply_core')
def apply_compile_go(self):
	try:
		nodes = self.go_nodes
	except AttributeError:
		self.go_compile_task = None
	else:
		self.go_compile_task = self.create_task('gocompile',
			nodes,
			[self.path.find_or_declare(self.target + self.env.GO_EXTENSION)])

@feature('gopackage', 'goprogram')
@after('apply_compile_go')
def apply_goinc(self):
	if not getattr(self, 'go_compile_task', None):
		return

	names = self.to_list(getattr(self, 'uselib_local', []))
	for name in names:
		obj = self.name_to_obj(name)
		if not obj:
			raise Utils.WafError('object %r was not found in uselib_local '
					'(required by %r)' % (lib_name, self.name))
		obj.post()
		self.go_compile_task.set_run_after(obj.go_package_task)
		self.go_compile_task.deps_nodes.extend(obj.go_package_task.outputs)
		self.env.append_unique('GOCFLAGS', '-I' + obj.path.abspath(obj.env))
		self.env.append_unique('GOLFLAGS', '-L' + obj.path.abspath(obj.env))

@feature('gopackage')
@after('apply_goinc')
def apply_gopackage(self):
	self.go_package_task = self.create_task('gopack',
			self.go_compile_task.outputs[0],
			self.path.find_or_declare(self.target + self.env.GO_PACK_EXTENSION))
	self.go_package_task.set_run_after(self.go_compile_task)
	self.go_package_task.deps_nodes.extend(self.go_compile_task.outputs)

@feature('goprogram')
@after('apply_goinc')
def apply_golink(self):
	self.go_link_task = self.create_task('golink',
			self.go_compile_task.outputs[0],
			self.path.find_or_declare(self.target))
	self.go_link_task.set_run_after(self.go_compile_task)
	self.go_link_task.deps_nodes.extend(self.go_compile_task.outputs)

