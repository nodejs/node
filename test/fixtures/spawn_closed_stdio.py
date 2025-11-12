import os
import sys
import subprocess
os.close(0)
os.close(1)
os.close(2)
exit_code = subprocess.call(sys.argv[1:], shell=False)
sys.exit(exit_code)
