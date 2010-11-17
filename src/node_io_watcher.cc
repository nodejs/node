// Copyright 2009 Ryan Dahl <ry@tinyclouds.org>
#include <node_io_watcher.h>

#include <node.h>
#include <node_buffer.h>
#include <v8.h>


#include <sys/uio.h> /* writev */
#include <errno.h>
#include <limits.h> /* IOV_MAX */

#include <sys/types.h>
#include <sys/socket.h>


#include <assert.h>

namespace node {

using namespace v8;

static ev_prepare dumper;
static Persistent<Object> dump_queue;

Persistent<FunctionTemplate> IOWatcher::constructor_template;
Persistent<String> callback_symbol;

static Persistent<String> next_sym;
static Persistent<String> prev_sym;
static Persistent<String> ondrain_sym;
static Persistent<String> onerror_sym;
static Persistent<String> data_sym;
static Persistent<String> encoding_sym;
static Persistent<String> offset_sym;
static Persistent<String> fd_sym;
static Persistent<String> is_unix_socket_sym;
static Persistent<String> first_bucket_sym;
static Persistent<String> last_bucket_sym;
static Persistent<String> queue_size_sym;
static Persistent<String> callback_sym;


void IOWatcher::Initialize(Handle<Object> target) {
  HandleScope scope;

  Local<FunctionTemplate> t = FunctionTemplate::New(IOWatcher::New);
  constructor_template = Persistent<FunctionTemplate>::New(t);
  constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
  constructor_template->SetClassName(String::NewSymbol("IOWatcher"));

  NODE_SET_PROTOTYPE_METHOD(constructor_template, "start", IOWatcher::Start);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "stop", IOWatcher::Stop);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "set", IOWatcher::Set);

  Local<Function> io_watcher = constructor_template->GetFunction();
  target->Set(String::NewSymbol("IOWatcher"), io_watcher);

  NODE_SET_METHOD(constructor_template->GetFunction(),
                  "flush",
                  IOWatcher::Flush);

  callback_symbol = NODE_PSYMBOL("callback");

  next_sym = NODE_PSYMBOL("next");
  prev_sym = NODE_PSYMBOL("prev");
  ondrain_sym = NODE_PSYMBOL("ondrain");
  onerror_sym = NODE_PSYMBOL("onerror");
  first_bucket_sym = NODE_PSYMBOL("firstBucket");
  last_bucket_sym = NODE_PSYMBOL("lastBucket");
  queue_size_sym = NODE_PSYMBOL("queueSize");
  offset_sym = NODE_PSYMBOL("offset");
  fd_sym = NODE_PSYMBOL("fd");
  is_unix_socket_sym = NODE_PSYMBOL("isUnixSocket");
  data_sym = NODE_PSYMBOL("data");
  encoding_sym = NODE_PSYMBOL("encoding");
  callback_sym = NODE_PSYMBOL("callback");


  ev_prepare_init(&dumper, IOWatcher::Dump);
  ev_prepare_start(EV_DEFAULT_UC_ &dumper);
  // Need to make sure that Dump runs *after* all other prepare watchers -
  // in particular the next tick one.
  ev_set_priority(&dumper, EV_MINPRI);
  ev_unref(EV_DEFAULT_UC);

  dump_queue = Persistent<Object>::New(Object::New());
  io_watcher->Set(String::NewSymbol("dumpQueue"), dump_queue);
}


void IOWatcher::Callback(EV_P_ ev_io *w, int revents) {
  IOWatcher *io = static_cast<IOWatcher*>(w->data);
  assert(w == &io->watcher_);
  HandleScope scope;

  Local<Value> callback_v = io->handle_->Get(callback_symbol);
  if (!callback_v->IsFunction()) {
    io->Stop();
    return;
  }

  Local<Function> callback = Local<Function>::Cast(callback_v);

  TryCatch try_catch;

  Local<Value> argv[2];
  argv[0] = Local<Value>::New(revents & EV_READ ? True() : False());
  argv[1] = Local<Value>::New(revents & EV_WRITE ? True() : False());

  callback->Call(io->handle_, 2, argv);

  if (try_catch.HasCaught()) {
    FatalException(try_catch);
  }
}


//
//  var io = new process.IOWatcher();
//  process.callback = function (readable, writable) { ... };
//  io.set(fd, true, false);
//  io.start();
//
Handle<Value> IOWatcher::New(const Arguments& args) {
  if (!args.IsConstructCall()) {
    return FromConstructorTemplate(constructor_template, args);
  }

  HandleScope scope;
  IOWatcher *s = new IOWatcher();
  s->Wrap(args.This());
  return args.This();
}


Handle<Value> IOWatcher::Start(const Arguments& args) {
  HandleScope scope;
  IOWatcher *io = ObjectWrap::Unwrap<IOWatcher>(args.Holder());
  io->Start();
  return Undefined();
}


Handle<Value> IOWatcher::Stop(const Arguments& args) {
  HandleScope scope;
  IOWatcher *io = ObjectWrap::Unwrap<IOWatcher>(args.Holder());
  io->Stop();
  return Undefined();
}


void IOWatcher::Start() {
  if (!ev_is_active(&watcher_)) {
    ev_io_start(EV_DEFAULT_UC_ &watcher_);
    Ref();
  }
}


void IOWatcher::Stop() {
  if (ev_is_active(&watcher_)) {
    ev_io_stop(EV_DEFAULT_UC_ &watcher_);
    Unref();
  }
}


Handle<Value> IOWatcher::Set(const Arguments& args) {
  HandleScope scope;

  IOWatcher *io = ObjectWrap::Unwrap<IOWatcher>(args.Holder());

  if (!args[0]->IsInt32()) {
    return ThrowException(Exception::TypeError(
          String::New("First arg should be a file descriptor.")));
  }

  int fd = args[0]->Int32Value();

  if (!args[1]->IsBoolean()) {
    return ThrowException(Exception::TypeError(
          String::New("Second arg should boolean (readable).")));
  }

  int events = 0;

  if (args[1]->IsTrue()) events |= EV_READ;

  if (!args[2]->IsBoolean()) {
    return ThrowException(Exception::TypeError(
          String::New("Third arg should boolean (writable).")));
  }

  if (args[2]->IsTrue()) events |= EV_WRITE;

  assert(!io->watcher_.active);
  ev_io_set(&io->watcher_, fd, events);

  return Undefined();
}


Handle<Value> IOWatcher::Flush(const Arguments& args) {
  HandleScope scope; // unneccessary?
  IOWatcher::Dump();
  return Undefined();
}

#define KB 1024

/*
 * A large javascript object structure is built up in net.js. The function
 * Dump is called at the end of each iteration, before select() is called,
 * to push all the data out to sockets.
 *
 * The structure looks like this:
 *
 * IOWatcher . dumpQueue
 *               |
 *               watcher . buckets - b - b - b - b
 *               |
 *               watcher . buckets - b - b
 *               |
 *               watcher . buckets - b
 *               |
 *               watcher . buckets - b - b - b
 *
 * The 'b' nodes are little javascript objects buckets. Each has a 'data'
 * member. 'data' is either a string or buffer. E.G.
 *
 *   b = { data: "hello world" }
 *
 */

// To enable this debug output, add '-DDUMP_DEBUG' to CPPFLAGS
// in 'build/c4che/default.cache.py' and 'make clean all'
#ifdef DUMP_DEBUG
#define DEBUG_PRINT(fmt,...) \
  fprintf(stderr, "(dump:%d) " fmt "\n",  __LINE__, ##__VA_ARGS__)
#else
#define DEBUG_PRINT(fmt,...)
#endif


void IOWatcher::Dump(EV_P_ ev_prepare *w, int revents) {
  assert(revents == EV_PREPARE);
  assert(w == &dumper);
  Dump();
}


void IOWatcher::Dump() {
  HandleScope scope;

  static struct iovec iov[IOV_MAX];

  // Loop over all watchers in the dump queue. Each one stands for a socket
  // that has stuff to be written out.
  //
  // There are several possible outcomes for each watcher.
  // 1. All the buckets associated with the watcher are written out. In this
  //    case the watcher is disabled; it is removed from the dump_queue.
  // 2. Some of the data was written, but there still remains buckets. In
  //    this case the watcher is enabled (i.e. we wait for the file
  //    descriptor to become readable) and we remove it from the dump_queue.
  //    When it becomes readable, we'll get a callback in net.js and add it
  //    again to the dump_queue
  // 3. writev returns EAGAIN. This is the same as case 2.
  //
  // In any case, the dump queue should be empty when we exit this function.
  // (See the assert at the end of the outermost for loop.
  Local<Value> watcher_v;
  Local<Object> watcher;

  for (watcher_v = dump_queue->Get(next_sym);
       watcher_v->IsObject();
       dump_queue->Set(next_sym, (watcher_v = watcher->Get(next_sym))),
       watcher->Set(next_sym, Null())) {
    watcher = watcher_v->ToObject();

    IOWatcher *io = ObjectWrap::Unwrap<IOWatcher>(watcher);

    // stats (just for fun)
    io->dumps_++;
    io->last_dump_ = ev_now(EV_DEFAULT_UC);

    DEBUG_PRINT("dumping fd %d", io->watcher_.fd);

    // Number of items we've stored in iov
    int iovcnt = 0;
    // Number of bytes we've stored in iov
    size_t to_write = 0;

    bool unix_socket = false;
    if (watcher->Has(is_unix_socket_sym) && watcher->Get(is_unix_socket_sym)->IsTrue()) {
      unix_socket = true;
    }

    // Unix sockets don't like huge messages. TCP sockets do.
    // TODO: handle EMSGSIZE after sendmsg().
    size_t max_to_write = unix_socket ? 8*KB : 64*KB;

    int fd_to_send = -1;

    // Offset is only as large as the first buffer of data. (See assert
    // below) Offset > 0 occurs when a previous writev could not entirely
    // drain a bucket.
    size_t offset = 0;
    if (watcher->Has(offset_sym)) {
      offset = watcher->Get(offset_sym)->Uint32Value();
    }
    size_t first_offset = offset;


    // Loop over all the buckets for this particular watcher/socket in order
    // to fill iov.
    Local<Value> bucket_v;
    Local<Object> bucket;
    unsigned int bucket_index = 0;

    for (bucket_v = watcher->Get(first_bucket_sym);
           // Break if we have an FD to send.
           // sendmsg can only handle one FD at a time.
           fd_to_send < 0 &&
           // break if we've hit the end
           bucket_v->IsObject() &&
           // break if iov contains a lot of data
           to_write < max_to_write &&
           // break if iov is running out of space
           iovcnt < IOV_MAX;
         bucket_v = bucket->Get(next_sym), bucket_index++) {
      assert(bucket_v->IsObject());
      bucket = bucket_v->ToObject();

      Local<Value> data_v = bucket->Get(data_sym);
      // net.js will be setting this 'data' value. We can ensure that it is
      // never empty.
      assert(!data_v.IsEmpty());

      Local<Object> buf_object;

      if (data_v->IsString()) {
        // TODO: insert v8::String::Pointers() hack here.
        Local<String> s = data_v->ToString();
        Local<Value> e = bucket->Get(encoding_sym);
        buf_object = Local<Object>::New(Buffer::New(s, e));
        bucket->Set(data_sym, buf_object);
      } else if (Buffer::HasInstance(data_v)) {
        buf_object = data_v->ToObject();
      } else {
        assert(0);
      }

      size_t l = Buffer::Length(buf_object);

      if (l == 0) continue;

      assert(first_offset < l);
      iov[iovcnt].iov_base = Buffer::Data(buf_object) + first_offset;
      iov[iovcnt].iov_len = l - first_offset;
      to_write += iov[iovcnt].iov_len;
      iovcnt++;

      first_offset = 0; // only the first buffer will be offset.

      if (unix_socket && bucket->Has(fd_sym)) {
        Local<Value> fd_v = bucket->Get(fd_sym);
        if (fd_v->IsInt32()) {
          fd_to_send = fd_v->Int32Value();
          DEBUG_PRINT("got fd to send: %d", fd_to_send);
          assert(fd_to_send >= 0);
        }
      }
    }

    if (to_write > 0) {
      ssize_t written;

      if (unix_socket) {
        struct msghdr msg;
        char scratch[64];

        msg.msg_name = NULL;
        msg.msg_namelen = 0;
        msg.msg_iov = iov;
        msg.msg_iovlen = iovcnt;
        msg.msg_control = NULL; // void*
        msg.msg_controllen = 0; // socklen_t
        msg.msg_flags = 0; // int

        if (fd_to_send >= 0) {
          struct cmsghdr *cmsg;

          msg.msg_control = (void *) scratch;
          msg.msg_controllen = CMSG_LEN(sizeof(fd_to_send));

          cmsg = CMSG_FIRSTHDR(&msg);
          cmsg->cmsg_level = SOL_SOCKET;
          cmsg->cmsg_type = SCM_RIGHTS;
          cmsg->cmsg_len = msg.msg_controllen;
          *(int*) CMSG_DATA(cmsg) = fd_to_send;
        }

        written = sendmsg(io->watcher_.fd, &msg, 0);
      } else {
        written = writev(io->watcher_.fd, iov, iovcnt);
      }

      DEBUG_PRINT("iovcnt: %d, to_write: %ld, written: %ld",
                  iovcnt,
                  to_write,
                  written);

      if (written < 0) {
        // Allow EAGAIN.
        // TODO: handle EMSGSIZE after sendmsg().
        if (errno == EAGAIN) {
          io->Start();
        } else {
          // Emit error event
          if (watcher->Has(onerror_sym)) {
            Local<Value> callback_v = io->handle_->Get(onerror_sym);
            assert(callback_v->IsFunction());
            Local<Function> callback = Local<Function>::Cast(callback_v);

            Local<Value> argv[1] = { Integer::New(errno) };

            TryCatch try_catch;

            callback->Call(io->handle_, 1, argv);

            if (try_catch.HasCaught()) {
              FatalException(try_catch);
            }
          }
        }
        // Continue with the next watcher.
        continue;
      }

      // what about written == 0 ?

      size_t queue_size = watcher->Get(queue_size_sym)->Uint32Value();
      assert(queue_size >= offset);

      // Now drop the buckets that have been written.
      bucket_index = 0;

      while (written > 0) {
        bucket_v = watcher->Get(first_bucket_sym);
        if (!bucket_v->IsObject()) {
          // No more buckets in the queue. Make sure the last_bucket_sym is
          // updated and then go to the next watcher.
          watcher->Set(last_bucket_sym, Null());
          break;
        }

        bucket = bucket_v->ToObject();

        Local<Value> data_v = bucket->Get(data_sym);
        assert(!data_v.IsEmpty());

        // At the moment we're turning all string into buffers
        // so we assert that this is not a string. However, when the
        // "Pointer patch" lands, this assert will need to be removed.
        assert(!data_v->IsString());
        // When the "Pointer patch" lands, we will need to be careful
        // to somehow store the length of strings that we're optimizing on
        // so that it need not be recalculated here. Note the "Pointer patch"
        // will only apply to ASCII strings - UTF8 ones will need to be
        // serialized onto a buffer.
        size_t bucket_len = Buffer::Length(data_v->ToObject());

        if (unix_socket && bucket->Has(fd_sym)) {
          bucket->Set(fd_sym, Null());
        }

        assert(bucket_len > offset);
        DEBUG_PRINT("[%ld] bucket_len: %ld, offset: %ld", bucket_index, bucket_len, offset);

        queue_size -= written;

        // Only on the first bucket does is the offset > 0.
        if (offset + written < bucket_len) {
          // we have not written the entire bucket
          DEBUG_PRINT("[%ld] Only wrote part of the buffer. "
                      "setting watcher.offset = %ld",
                      bucket_index,
                      offset + written);

          watcher->Set(offset_sym,
                           Integer::NewFromUnsigned(offset + written));
          break;
        } else {
          DEBUG_PRINT("[%ld] wrote the whole bucket. discarding.",
                      bucket_index);

          written -= bucket_len - offset;

          Local<Value> bucket_callback_v = bucket->Get(callback_sym);
          if (bucket_callback_v->IsFunction()) {
            Local<Function> bucket_callback =
              Local<Function>::Cast(bucket_callback_v);
            TryCatch try_catch;
            bucket_callback->Call(io->handle_, 0, NULL);
            if (try_catch.HasCaught()) {
              FatalException(try_catch);
            }
          }

          // Offset is now zero
          watcher->Set(offset_sym, Integer::NewFromUnsigned(0));
        }

        offset = 0; // the next bucket will have zero offset;
        bucket_index++;

        // unshift
        watcher->Set(first_bucket_sym, bucket->Get(next_sym));
      }

      // Set the queue size.
      watcher->Set(queue_size_sym, Integer::NewFromUnsigned(queue_size));
    }


    // Finished dumping the buckets.
    //
    // If our list of buckets is empty, we can emit 'drain' and forget about
    // this socket. Nothing needs to be done.
    //
    // Otherwise we need to prepare the io_watcher to wait for the interface
    // to become writable again.

    if (watcher->Get(first_bucket_sym)->IsObject()) {
      // Still have buckets to be written. Wait for fd to become writable.
      io->Start();

      DEBUG_PRINT("Started watcher %d", io->watcher_.fd);
    } else {
      // No more buckets in the queue. Make sure the last_bucket_sym is
      // updated and then go to the next watcher.
      watcher->Set(last_bucket_sym, Null());

      // Emptied the buckets queue for this socket.  Don't wait for it to
      // become writable.
      io->Stop();

      DEBUG_PRINT("Stop watcher %d", io->watcher_.fd);

      // Emit drain event
      if (watcher->Has(ondrain_sym)) {
        Local<Value> callback_v = io->handle_->Get(ondrain_sym);
        assert(callback_v->IsFunction());
        Local<Function> callback = Local<Function>::Cast(callback_v);

        TryCatch try_catch;

        callback->Call(io->handle_, 0, NULL);

        if (try_catch.HasCaught()) {
          FatalException(try_catch);
        }
      }
    }
  }

  // Assert that the dump_queue is empty.
  assert(!dump_queue->Get(next_sym)->IsObject());
}


}  // namespace node
