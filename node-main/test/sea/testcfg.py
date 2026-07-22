import sys, os, multiprocessing, shutil
sys.path.append(os.path.join(os.path.dirname(__file__), '..'))
import testpy

def GetConfiguration(context, root):
  # We don't use arch-specific out folder in Node.js; use none/none to auto
  # detect release mode from GetVm and get the path to the executable.
  vm = context.GetVm('none', 'none')
  if not os.path.isfile(vm):
    return testpy.SimpleTestConfiguration(context, root, 'sea')

  # Get the size of the executable to decide whether we can run tests in parallel.
  executable_size = os.path.getsize(vm)
  num_cpus = multiprocessing.cpu_count()
  remaining_disk_space = shutil.disk_usage('.').free
  # Give it a bit of leeway by multiplying by 3.
  if (executable_size * num_cpus * 3 > remaining_disk_space):
    return testpy.SimpleTestConfiguration(context, root, 'sea')

  return testpy.ParallelTestConfiguration(context, root, 'sea')
