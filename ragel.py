#! /usr/bin/env python
# encoding: utf-8

"Ragel: '.rl' files are converted into .c files using 'ragel': {.rl -> .c -> .o}"

import TaskGen, Task, Runner


def rageltaskfun(task):
	env = task.env
	ragelbin = env.get_flat('RAGEL')
	if ragelbin:
		if task.inputs[0].srcpath(env) == '../src/config_parser.rl':
			cmd = '%s -o %s -C -T0 %s' % (ragelbin, task.outputs[0].bldpath(env), task.inputs[0].srcpath(env))
		else:
			cmd = '%s -o %s -C -T1 %s' % (ragelbin, task.outputs[0].bldpath(env), task.inputs[0].srcpath(env))
	else:
		src = task.inputs[0].srcpath(env)
		src = src[:src.rfind('.')] + '.c'
		cmd = 'cp %s %s' % (src, task.outputs[0].bldpath(env))
	return task.generator.bld.exec_command(cmd)

rageltask = Task.task_type_from_func('ragel', rageltaskfun, vars = ['RAGEL'], color = 'BLUE', ext_in = '.rl', ext_out = '.c', before = 'c')

@TaskGen.extension('.rl')
@TaskGen.before('apply_core')
def ragel(self, node):
	out = node.change_ext('.c')
	self.allnodes.append(out)
	tsk = self.create_task('ragel')
	tsk.set_inputs(node)
	tsk.set_outputs(out)

def detect(conf):
	dang = conf.find_program('ragel', var='RAGEL')
