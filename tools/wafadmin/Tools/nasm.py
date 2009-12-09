#!/usr/bin/env python
# encoding: utf-8
# Thomas Nagy, 2008

"""
Nasm processing
"""

import os
import TaskGen, Task, Utils
from TaskGen import taskgen, before, extension

nasm_str = '${NASM} ${NASM_FLAGS} ${NASM_INCLUDES} ${SRC} -o ${TGT}'

EXT_NASM = ['.s', '.S', '.asm', '.ASM', '.spp', '.SPP']

@before('apply_link')
def apply_nasm_vars(self):

	# flags
	if hasattr(self, 'nasm_flags'):
		for flag in self.to_list(self.nasm_flags):
			self.env.append_value('NASM_FLAGS', flag)

	# includes - well, if we suppose it works with c processing
	if hasattr(self, 'includes'):
		for inc in self.to_list(self.includes):
			node = self.path.find_dir(inc)
			if not node:
				raise Utils.WafError('cannot find the dir' + inc)
			self.env.append_value('NASM_INCLUDES', '-I%s' % node.srcpath(self.env))
			self.env.append_value('NASM_INCLUDES', '-I%s' % node.bldpath(self.env))

@extension(EXT_NASM)
def nasm_file(self, node):
	try: obj_ext = self.obj_ext
	except AttributeError: obj_ext = '_%d.o' % self.idx

 	task = self.create_task('nasm', node, node.change_ext(obj_ext))
	self.compiled_tasks.append(task)

	self.meths.append('apply_nasm_vars')

# create our action here
Task.simple_task_type('nasm', nasm_str, color='BLUE', ext_out='.o', shell=False)

def detect(conf):
	nasm = conf.find_program(['nasm', 'yasm'], var='NASM', mandatory=True)

