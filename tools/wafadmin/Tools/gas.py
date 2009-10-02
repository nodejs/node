#!/usr/bin/env python
# encoding: utf-8
# Thomas Nagy, 2008 (ita)

"as and gas"

import os, sys
import Task
from TaskGen import extension, taskgen, after, before

EXT_ASM = ['.s', '.S', '.asm', '.ASM', '.spp', '.SPP']

as_str = '${AS} ${ASFLAGS} ${_ASINCFLAGS} ${SRC} -o ${TGT}'
Task.simple_task_type('asm', as_str, 'PINK', ext_out='.o', shell=False)

@extension(EXT_ASM)
def asm_hook(self, node):
	# create the compilation task: cpp or cc
	task = self.create_task('asm')
	try: obj_ext = self.obj_ext
	except AttributeError: obj_ext = '_%d.o' % self.idx

	task.inputs = [node]
	task.outputs = [node.change_ext(obj_ext)]
	self.compiled_tasks.append(task)
	self.meths.append('asm_incflags')

@taskgen
@after('apply_obj_vars_cc')
@after('apply_obj_vars_cxx')
@before('apply_link')
def asm_incflags(self):
	if self.env['ASINCFLAGS']: self.env['_ASINCFLAGS'] = self.env['ASINCFLAGS']
	if 'cxx' in self.features: self.env['_ASINCFLAGS'] = self.env['_CXXINCFLAGS']
	else: self.env['_ASINCFLAGS'] = self.env['_CCINCFLAGS']

def detect(conf):
	conf.find_program(['gas', 'as'], var='AS')
	if not conf.env.AS: conf.env.AS = conf.env.CC

