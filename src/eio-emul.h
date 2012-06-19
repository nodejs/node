/* Copyright Joyent, Inc. and other Node contributors.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the
 * following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
 * NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef NODE_EIO_EMUL_H_
#define NODE_EIO_EMUL_H_

#define eio_nop(a,b,c)                      eio_nop(a,b,c,(&uv_default_loop()->uv_eio_channel))
#define eio_busy(a,b,c,d)                   eio_busy(a,b,c,d,(&uv_default_loop()->uv_eio_channel))
#define eio_sync(a,b,c)                     eio_sync(a,b,c,(&uv_default_loop()->uv_eio_channel))
#define eio_fsync(a,b,c,d)                  eio_fsync(a,b,c,d,(&uv_default_loop()->uv_eio_channel))
#define eio_msync(a,b,c,d,e,f)              eio_msync(a,b,c,d,e,f,(&uv_default_loop()->uv_eio_channel))
#define eio_fdatasync(a,b,c,d)              eio_fdatasync(a,b,c,d,(&uv_default_loop()->uv_eio_channel))
#define eio_syncfs(a,b,c,d)                 eio_syncfs(a,b,c,d,(&uv_default_loop()->uv_eio_channel))
#define eio_sync_file_range(a,b,c,d,e,f,g)  eio_sync_file_range(a,b,c,d,e,f,g,(&uv_default_loop()->uv_eio_channel))
#define eio_mtouch(a,b,c,d,e,f)             eio_mtouch(a,b,c,d,e,f,(&uv_default_loop()->uv_eio_channel))
#define eio_mlock(a,b,c,d,e)                eio_mlock(a,b,c,d,e,(&uv_default_loop()->uv_eio_channel))
#define eio_mlockall(a,b,c,d)               eio_mlockall(a,b,c,d,(&uv_default_loop()->uv_eio_channel))
#define eio_fallocate(a,b,c,d,e,f,g)        eio_fallocate(a,b,c,d,e,f,g,(&uv_default_loop()->uv_eio_channel))
#define eio_close(a,b,c,d)                  eio_close(a,b,c,d,(&uv_default_loop()->uv_eio_channel))
#define eio_readahead(a,b,c,d,e,f)          eio_readahead(a,b,c,d,e,f,(&uv_default_loop()->uv_eio_channel))
#define eio_read(a,b,c,d,e,f,g)             eio_read(a,b,c,d,e,f,g,(&uv_default_loop()->uv_eio_channel))
#define eio_write(a,b,c,d,e,f,g)            eio_write(a,b,c,d,e,f,g,(&uv_default_loop()->uv_eio_channel))
#define eio_fstat(a,b,c,d)                  eio_fstat(a,b,c,d,(&uv_default_loop()->uv_eio_channel))
#define eio_fstatvfs(a,b,c,d)               eio_fstatvfs(a,b,c,d,(&uv_default_loop()->uv_eio_channel))
#define eio_futime(a,b,c,d,e,f)             eio_futime(a,b,c,d,e,f,(&uv_default_loop()->uv_eio_channel))
#define eio_ftruncate(a,b,c,d,e)            eio_ftruncate(a,b,c,d,e,(&uv_default_loop()->uv_eio_channel))
#define eio_fchmod(a,b,c,d,e)               eio_fchmod(a,b,c,d,e,(&uv_default_loop()->uv_eio_channel))
#define eio_fchown(a,b,c,d,e,f)             eio_fchown(a,b,c,d,e,f,(&uv_default_loop()->uv_eio_channel))
#define eio_dup2(a,b,c,d,e)                 eio_dup2(a,b,c,d,e,(&uv_default_loop()->uv_eio_channel))
#define eio_sendfile(a,b,c,d,e,f,g)         eio_sendfile(a,b,c,d,e,f,g,(&uv_default_loop()->uv_eio_channel))
#define eio_open(a,b,c,d,e,f)               eio_open(a,b,c,d,e,f,(&uv_default_loop()->uv_eio_channel))
#define eio_utime(a,b,c,d,e,f)              eio_utime(a,b,c,d,e,f,(&uv_default_loop()->uv_eio_channel))
#define eio_truncate(a,b,c,d,e)             eio_truncate(a,b,c,d,e,(&uv_default_loop()->uv_eio_channel))
#define eio_chown(a,b,c,d,e,f)              eio_chown(a,b,c,d,e,f,(&uv_default_loop()->uv_eio_channel))
#define eio_chmod(a,b,c,d,e)                eio_chmod(a,b,c,d,e,(&uv_default_loop()->uv_eio_channel))
#define eio_mkdir(a,b,c,d,e)                eio_mkdir(a,b,c,d,e,(&uv_default_loop()->uv_eio_channel))
#define eio_readlink(a,b,c,d)               eio_readlink(a,b,c,d,(&uv_default_loop()->uv_eio_channel))
#define eio_realpath(a,b,c,d)               eio_realpath(a,b,c,d,(&uv_default_loop()->uv_eio_channel))
#define eio_stat(a,b,c,d)                   eio_stat(a,b,c,d,(&uv_default_loop()->uv_eio_channel))
#define eio_lstat(a,b,c,d)                  eio_lstat(a,b,c,d,(&uv_default_loop()->uv_eio_channel))
#define eio_statvfs(a,b,c,d)                eio_statvfs(a,b,c,d,(&uv_default_loop()->uv_eio_channel))
#define eio_unlink(a,b,c,d)                 eio_unlink(a,b,c,d,(&uv_default_loop()->uv_eio_channel))
#define eio_rmdir(a,b,c,d)                  eio_rmdir(a,b,c,d,(&uv_default_loop()->uv_eio_channel))
#define eio_readdir(a,b,c,d,e)              eio_readdir(a,b,c,d,e,(&uv_default_loop()->uv_eio_channel))
#define eio_mknod(a,b,c,d,e,f)              eio_mknod(a,b,c,d,e,f,(&uv_default_loop()->uv_eio_channel))
#define eio_link(a,b,c,d,e)                 eio_link(a,b,c,d,e,(&uv_default_loop()->uv_eio_channel))
#define eio_symlink(a,b,c,d,e)              eio_symlink(a,b,c,d,e,(&uv_default_loop()->uv_eio_channel))
#define eio_rename(a,b,c,d,e)               eio_rename(a,b,c,d,e,(&uv_default_loop()->uv_eio_channel))
#define eio_custom(a,b,c,d)                 eio_custom((void (*)(eio_req*))a,b,c,d,(&uv_default_loop()->uv_eio_channel))
#define eio_grp(a,b)                        eio_grp(a,b,(&uv_default_loop()->uv_eio_channel))

#endif /* NODE_EIO_EMUL_H_ */
