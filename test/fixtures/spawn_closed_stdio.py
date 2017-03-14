import os
import sys
import subprocess
os.close(0)
os.close(1)
os.close(2)
cmd = sys.argv[1] + ' -e "process.stdin; process.stdout; process.stderr;"'
exit_code = subprocess.call(cmd, shell=False)
sys.exit(exit_code)
