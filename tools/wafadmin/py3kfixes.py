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
		lst = []
		for y in '. Tools 3rdparty'.split():
			for x in os.listdir(os.path.join(dir, y)):
				if x.endswith('.py'):
					lst.append(y + os.sep + x)
		#lst = [y + os.sep + x for x in os.listdir(os.path.join(dir, y)) for y in '. Tools 3rdparty'.split() if x.endswith('.py')]
		for x in lst:
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
	code = code.replace('p.communicate()[0]', 'p.communicate()[0].decode("utf-8")')
	return code

@subst('Utils.py')
def r3(code):
	code = code.replace("m.update(str(lst))", "m.update(str(lst).encode())")
	code = code.replace('p.communicate()[0]', 'p.communicate()[0].decode("utf-8")')
	return code

@subst('ansiterm.py')
def r33(code):
	code = code.replace('unicode', 'str')
	return code

@subst('Task.py')
def r4(code):
	code = code.replace("up(self.__class__.__name__)", "up(self.__class__.__name__.encode())")
	code = code.replace("up(self.env.variant())", "up(self.env.variant().encode())")
	code = code.replace("up(x.parent.abspath())", "up(x.parent.abspath().encode())")
	code = code.replace("up(x.name)", "up(x.name.encode())")
	code = code.replace('class TaskBase(object):\n\t__metaclass__=store_task_type', 'import binascii\n\nclass TaskBase(object, metaclass=store_task_type):')
	code = code.replace('keys=self.cstr_groups.keys()', 'keys=list(self.cstr_groups.keys())')
	code = code.replace("sig.encode('hex')", 'binascii.hexlify(sig)')
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

@subst('Tools/python.py')
def r8(code):
	code = code.replace('proc.communicate()[0]', 'proc.communicate()[0].decode("utf-8")')
	return code

@subst('Tools/glib2.py')
def r9(code):
	code = code.replace('f.write(c)', 'f.write(c.encode("utf-8"))')
	return code

@subst('Tools/config_c.py')
def r10(code):
	code = code.replace("key=kw['success']", "key=kw['success']\n\t\t\t\ttry:\n\t\t\t\t\tkey=key.decode('utf-8')\n\t\t\t\texcept:\n\t\t\t\t\tpass")
	return code

def fixdir(dir):
	global all_modifs
	for k in all_modifs:
		for v in all_modifs[k]:
			modif(os.path.join(dir, 'wafadmin'), k, v)
	#print('substitutions finished')

