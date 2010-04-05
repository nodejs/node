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
	try: obj_ext = self.obj_ext
	except AttributeError: obj_ext = '_%d.o' % self.idx

	task = self.create_task('asm', node, node.change_ext(obj_ext))
	self.compiled_tasks.append(task)
	self.meths.append('asm_incflags')

@after('apply_obj_vars_cc')
@after('apply_obj_vars_cxx')
@before('apply_link')
def asm_incflags(self):
	self.env.append_value('_ASINCFLAGS', self.env.ASINCFLAGS)
	var = ('cxx' in self.features) and 'CXX' or 'CC'
	self.env.append_value('_ASINCFLAGS', self.env['_%sINCFLAGS' % var])

def detect(conf):
	conf.find_program(['gas', 'as'], var='AS')
	if not conf.env.AS: conf.env.AS = conf.env.CC
	#conf.env.ASFLAGS = ['-c'] <- may be necesary for .S files

