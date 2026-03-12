import fs from 'node:fs'
import fsp from 'node:fs/promises'

const fns = fs.globSync('out/doc/api/*.html')

const scripts = '<script src="./add_toc_tree_to_leftnav.js"></script>';

const last_nbyte = 512
const buffer = Buffer.alloc(last_nbyte)
let no = 0

for (let fn of fns) {
  if (fn.endsWith('all.html')) continue;

  let fh
  try {
    fh = await fsp.open(fn, 'a+')
    let st = await fh.stat()
    if (st.size < last_nbyte) continue;

    await fh.read({buffer, position: st.size - last_nbyte})
    if (buffer.toString().includes(scripts)) continue;
    await fh.appendFile('\n' + scripts)
    console.log(` * append <script> to ${fn}`)
    no++
  } catch(e) {
    console.error(`ERROR: append <script> to ${fn} error ${e?.message}`)
  } finally {
    fh?.close()
  }

}

console.log(` -- add ${no} of ${fns.length} html files --`)

