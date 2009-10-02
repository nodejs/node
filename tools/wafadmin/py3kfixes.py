#!/usr/bin/env python
# encoding: utf-8
# Thomas Nagy, 2009 (ita)

"""
Fixes for py3k go here
"""

import os

all_modifs = {}

def modif(dir, name, fun):
	if name == '*':
		lst = os.listdir(dir) + ['Tools' + os.sep + x for x in os.listdir(os.path.join(dir, 'Tools'))]
		for x in lst:
			if x.endswith('.py'):
				modif(dir, x, fun)
		return

	filename = os.path.join(dir, name)
	f = open(filename, 'r')
	txt = f.read()
	f.close()

	txt = fun(txt)

	f = open(filename, 'w')
	f.write(txt)
	f.close()

def subst(filename):
	def do_subst(fun):
		global all_modifs
		try:
			all_modifs[filename] += fun
		except KeyError:
			all_modifs[filename] = [fun]
		return fun
	return do_subst

@subst('Constants.py')
def r1(code):
	code = code.replace("'iluvcuteoverload'", "b'iluvcuteoverload'")
	code = code.replace("ABI=7", "ABI=37")
	return code

@subst('Tools/ccroot.py')
def r2(code):
	code = code.replace("p.stdin.write('\\n')", "p.stdin.write(b'\\n')")
	code = code.replace("out=str(out)", "out=out.decode('utf-8')")
	return code

@subst('Utils.py')
def r3(code):
	code = code.replace("m.update(str(lst))", "m.update(str(lst).encode())")
	return code

@subst('Task.py')
def r4(code):
	code = code.replace("up(self.__class__.__name__)", "up(self.__class__.__name__.encode())")
	code = code.replace("up(self.env.variant())", "up(self.env.variant().encode())")
	code = code.replace("up(x.parent.abspath())", "up(x.parent.abspath().encode())")
	code = code.replace("up(x.name)", "up(x.name.encode())")
	code = code.replace('class TaskBase(object):\n\t__metaclass__=store_task_type', 'class TaskBase(object, metaclass=store_task_type):')
	code = code.replace('keys=self.cstr_groups.keys()', 'keys=list(self.cstr_groups.keys())')
	return code

@subst('Build.py')
def r5(code):
	code = code.replace("cPickle.dump(data,file,-1)", "cPickle.dump(data,file)")
	code = code.replace('for node in src_dir_node.childs.values():', 'for node in list(src_dir_node.childs.values()):')
	return code

@subst('*')
def r6(code):
	code = code.replace('xrange', 'range')
	code = code.replace('iteritems', 'items')
	code = code.replace('maxint', 'maxsize')
	code = code.replace('iterkeys', 'keys')
	code = code.replace('Error,e:', 'Error as e:')
	code = code.replace('Exception,e:', 'Exception as e:')
	return code

@subst('TaskGen.py')
def r7(code):
	code = code.replace('class task_gen(object):\n\t__metaclass__=register_obj', 'class task_gen(object, metaclass=register_obj):')
	return code

def fixdir(dir):
	global all_modifs
	for k in all_modifs:
		for v in all_modifs[k]:
			modif(os.path.join(dir, 'wafadmin'), k, v)
	#print('substitutions finished')
