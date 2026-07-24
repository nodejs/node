// Testing for issue 57780, works on MacOS and Windows,link here:
// https://github.com/nodejs/node/issues/57780

const child_process = require('child_process');
const iconv = require('iconv-lite'); // Still needed for Windows testing

const isWindows = process.platform === 'win32';

async function amain() {
  if (isWindows) {
    // Windows: Test cmd.exe with UTF-8 + GBK
    const cp = child_process.spawn('cmd', [], { 
      stdio: ['pipe', 'inherit', 'inherit'],
      env: { ...process.env, CHCP: '65001' } // Force UTF-8
    });

    const utf8Cmd = 'echo utf8中文测试\r\n';
    const gbkCmd = iconv.encode('echo gbk中文测试\r\n', 'gbk');
    
    cp.stdin.write(utf8Cmd);
    cp.stdin.write(gbkCmd);
    cp.stdin.end();
  } else {
    // macOS: Test UTF-8 in bash
    console.log('Testing UTF-8 on macOS:');
    const bash = child_process.spawn('bash', ['-c', 'echo "macOS中文测试"']);
    bash.stdout.pipe(process.stdout);
  }
}

amain().catch(console.error);