# Work in progress: New IO API notes

## Source

A `node::io::Source` holds data that is to be read. It exposes a single
`Pull()` method that can be called to either peek at or extract a chunk
of data. The caller passes in a buffer. The `Source` chooses to either
copy its data into that buffer, or return its own buffer. Whatever it
chooses, the data is returned to the caller by invoking the pull_cb.
The `Source` may provide its own callback (the push_cb) at that point
to be notified when the caller is done processing the data.

The Pull operation may be sync or async. The caller can require sync.

The Pull operation may be a peek (e.g. non-committing read)

The Source may buffer data if necessary.

### Example 1: Simple, synchronous, strict length read

In the following example, the caller is telling the source that it:

* Wants the pull to complete synchronously
* Wants the Source to copy the data into the provided buffer.

The source will return an error if it is not able to meet these restrictions.

```c
Source* mySource = GetSourceSomehow();
io_buf_t buffer;
buffer.base = Malloc<uint8_t>(100);
buffer.len = 100;

io_pull_t pull;

int status = mySource->Pull(&pull, &buffer.len, &buffer, 1, IO_PULL_FLAG_SYNC);

CHECK(status & IO_PULL_STATUS_SYNC);
if (status & IO_PULL_STATUS_ERROR) {
  int err = io_get_error(status);
  // do something with the error
  return;
}

// the data should be in buffer
```

### Example 2: Peek first, then read sync

```c
Source* mySource = GetSourceSomehow();

size_t length = 100;

io_pull_t pull;

// Peek to get the length
int status = mySource->Pull(&pull, &len, nullptr, 0,
                            IO_PULL_FLAG_SYNC |
                            IO_PULL_FLAG_PEEK);

CHECK(status & IO_PULL_STATUS_SYNC);
if (status & IO_PULL_STATUS_ERROR) {
  int err = io_get_error(status);
  // do something with the error
  return;
}

io_buf_t buffer;
buffer.base = Malloc<uint8_t>(length);
buffer.len = length;

status = mySource->Pull(&pull, &length, &buffer, 1, IO_PULL_FLAG_SYNC);

CHECK(status & IO_PULL_STATUS_SYNC);
if (status & IO_PULL_STATUS_ERROR) {
  int err = io_get_error(status);
  // do something with the error
  return;
}

// the data should be in buffer
```

### Example 3: Async pull, data copied into provided buffer

```c
Source* mySource = GetSourceSomehow();

io_buf_t buffer;
buffer.base = Malloc<uint8_t>(100);
buffer.len = length;

auto AfterPull = [](io_pull_t* handle,
                    int status,
                    io_buf_t* bufs,
                    size_t count,
                    size_t length,
                    io_push_t* source) {
  if (status & IO_PULL_STATUS_ERROR) {
    int error = io_get_error(status);
    // handle the error;
    return;
  }

  // data should be in bufs
};

io_pull_t pull;
io_pull_set_pull_cb(&pull, AfterPull);

int status = mySource->Pull(&pull, &buffer.len, &buffer, 1,
                            IO_PULL_FLAG_MUST_COPY);
CHECK(!(status & IO_PULL_STATUS_SYNC));
```

### Example 4: Async pull, source provided buffer

```c
Source* mySource = GetSourceSomehow();

io_buf_t buffer;
buffer.base = Malloc<uint8_t>(100);
buffer.len = length;

auto AfterPull = [](io_pull_t* handle,
                    int status,
                    io_buf_t* bufs,
                    size_t count,
                    size_t length,
                    io_push_t* source) {
  if (status & IO_PULL_STATUS_ERROR) {
    int error = io_get_error(status);
    // handle the error;
    return;
  }

  // data should be in bufs, handle it, then notify source that we're done
  auto cb = io_push_get_push_cb(source);
  if (cb != nullptr)
    cb(source, IO_PUSH_STATUS_OK);
};

io_pull_t pull;
io_pull_set_pull_cb(&pull, AfterPull);

int status = mySource->Pull(&pull, &buffer.len, &buffer, 1);
CHECK(!(status & IO_PULL_STATUS_SYNC));
```

### Example 5: Binding a Source and Sink

Binding is roughly the equivalent of `pipe()` in Readable, with the notable
exception that the Sink is pulling data from the Source, rather than being
pushed into by the Source.

```c
Source* mySource = GetSourceSomehow();
Sink* mySink = GetSinkSomehow();

auto BindCB = [](io_bind_t* handle, int status) {
  // handle the callback
  if (status & IO_BIND_STATUS_ERROR) {
    int error = io_get_error(status);
    // handle the error
    return;
  }

  Source* source = io_bind_get_source(handle);
  Sink* sink = io_bind_get_sink(handle);

  switch (status) {
    case IO_BIND_STATUS_WAIT:
      // the data flow was paused, use Sink->Signal() to restart flow
      break;
    default:
      // the data flow has completed. Source reached EOF
  }
};

io_bind_t bind;
io_bind_set_bind_cb(&bind, BindCB);

// Bind and signal the sink to begin pulling immediately
mySink->Bind(&bind, mySource, IO_BIND_FLAG_SIGNAL);
```

### Example 6: Hypothetical file compression and save

```c
FileSource* fileSource = getFileSourceSomehow();
CompressTransform* compress = getCompressionSomehow();
FileSink* fileSink = getFileSinkSomehow();

// Note, CompressTransform is both a Source and a Sink

io_bind_t read_bind;
io_bind_t save_bind;

// Set bind callbacks if we want....

compress->Bind(&read_bind, fileSource);
fileSink->Bind(&save_bind, compress);

fileSink.Signal(&save_bind);
```
