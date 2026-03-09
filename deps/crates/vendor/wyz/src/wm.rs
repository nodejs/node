/*! Waste Management

Do you drive your own garbage to the landfill or compost heap? Maybe you should,
but that’s a lot of work and takes time out of your day, so you probably don’t.
Instead, you give it to a worker that specializes in managing objects at the end
of their time in your service.

This module moves objects from the threads where they were working to a single,
global, worker thread when they go out of scope. Since an object that is going
out of scope can no longer be used, you could say that it is *garbage*; since
there is only one worker thread to receive all such objects, you could say that
the worker *collects* them. Wink wink, nudge nudge.

Users need only wrap their values in `BgDrop` to have their garbage collected.
`BgDrop` only accepts `'static` values, since the values are being sent to
another thread that makes no guarantees about timeliness of destruction, and
thus the garbage cannot have any lingering ties to live objects in the rest of
the program.

When a `BgDrop` goes out of scope, it attempts to send its interior value to the
collector thread. The first `BgDrop` to drop must start the collector thread,
which may result in an indefinite block until the thread begins. Once the
collector is running, all `BgDrop` drops will *attempt* to send their internal
value to the collector for destruction. If the send fails, then the value will
be dropped on the sending thread, rather than on the collector.

You can prevent future collections with `cancel_collection()`, which destroys
the channel used to move values to the collector thread. You can also get the
thread key for the collector with `collector()`. If you need to ensure that all
pending destructions occur before program exit, you should end your program with
a `cancel_collection()` and then `collector().unwrap().join()`. The collector
guarantees that objects queued for destruction are either enqueued for future
destruction *or* destroyed immediately, so the collector thread *will* receive a
signal for each object not destroyed on its prior thread.
!*/

#![cfg(all(feature = "std", feature = "garbage"))]

use once_cell::sync::OnceCell;

use tap::Pipe;

use std::{
	collections::VecDeque,
	marker::PhantomData,
	mem::{
		self,
		ManuallyDrop,
	},
	ops::{
		Deref,
		DerefMut,
	},
	sync::{
		mpsc,
		Mutex,
		MutexGuard,
		Once,
		RwLock,
	},
	thread,
};

use typemap::TypeMap;

/** Run an object’s destructor in the background.

When `BgDrop`-wrapped objects go out of scope, the `BgDrop` destructor attempts
to use a global background-thread to receive the wrapped value, so that its
destructor is run on the worker thread. If the thread running a `BgDrop`
destructor is able to send the value to the worker, then it resumes immediately,
and does not wait for the worker to get around to actually running the wrapped
destructor. This is similar to the disposal semantics of many GC systems, though
the actual system used to determine when an object becomes garbage is still the
compiler’s static lifetime analyzer.

All `BgDrop` types use the same persistent worker thread, minimizing the program
cost of deferral.

If the function [`wm::shutdown()`] is called, all future `BgDrop`s become a noöp
and run their contained destructors on their local threads.

[`wm::shutdown()`]: ../fn.shutdown.html
**/
#[repr(transparent)]
pub struct BgDrop<T: 'static> {
	inner: ManuallyDrop<T>,
}

impl<T: 'static> BgDrop<T> {
	/// Instructs an object to run its destructor in the background.
	///
	/// This function modifies the wrapped object’s `Drop` implementation to try
	/// to, on `drop`, send the inner object to a background thread for actual
	/// destruction. If the object cannot be sent to the background when its
	/// wrapper goes out of scope, then its destructor runs immediately, in the
	/// thread that had been holding the object when the modified destructor was
	/// called.
	///
	/// If the wrapped object is successfully sent to the background, the
	/// modified destructor exits, and the current thread resumes work. Once
	/// enqueued, the inner object is guaranteed to be *eventually* destroyed,
	/// unless the program exits in a manner that prevents the background
	/// collector from emptying its work queue.
	#[inline(always)]
	pub fn new(value: T) -> Self {
		Self {
			inner: ManuallyDrop::new(value),
		}
	}

	/// Removes the background-destruction marker, returning the interior value.
	#[inline(always)]
	pub fn into_inner(mut self) -> T {
		unsafe { ManuallyDrop::take(&mut self.inner) }
	}

	/// Attempt to prevent double-deferral, which would cause the outer to send
	/// the inner to the worker thread, making the worker thread send the
	/// *actual* inner to itself for destruction. This is safe, but stupid.
	#[inline(always)]
	#[doc(hidden)]
	pub fn bg_drop(self) -> Self {
		self
	}

	#[inline(always)]
	fn dtor(&mut self) {
		//  No destructor, no problem! Quit.
		if !mem::needs_drop::<T>() {
			return;
		}

		//  Ensure that the collector has been initialized.
		init();

		//  Pull the value into local scope, reärming the destructor.
		let val = unsafe { ManuallyDrop::take(&mut self.inner) };

		//  Get a local copy of the outbound channel, or exit.
		let sender = match sender() {
			Some(s) => s,
			None => return,
		};

		//  Enqueue the object into the transfer buffer.
		dq().entry::<Key<T>>()
			.or_insert_with(VecDeque::new)
			.pipe(|v| v.push_back(val));

		//  Send the dequeueïng destructor to the collector thread, or run it
		//  locally if the send failed.
		if sender.send(dtor::<T>).is_err() {
			dtor::<T>();
		}
	}
}

impl<T: 'static> AsRef<T> for BgDrop<T> {
	fn as_ref(&self) -> &T {
		&*self.inner
	}
}

impl<T: 'static> AsMut<T> for BgDrop<T> {
	fn as_mut(&mut self) -> &mut T {
		&mut *self.inner
	}
}

impl<T: 'static> Deref for BgDrop<T> {
	type Target = T;

	fn deref(&self) -> &Self::Target {
		&*self.inner
	}
}

impl<T: 'static> DerefMut for BgDrop<T> {
	fn deref_mut(&mut self) -> &mut Self::Target {
		&mut *self.inner
	}
}

impl<T: 'static> Drop for BgDrop<T> {
	fn drop(&mut self) {
		self.dtor();
	}
}

/// Attaches a `BgDrop` constructor to all suitable types.
pub trait BgDropExt: Sized + 'static {
	/// Modifies the object’s destructor to run in the background.
	///
	/// When this value goes out of scope, it will attempt to send itself to a
	/// background thread where its *actual* destructor will be run. The actual
	/// destructor will run on the local thread only if the transfer to the
	/// background worker was unable to occur.
	///
	/// The background worker is started only when the first value marked for
	/// deferred destruction actually drops, so the first call will block until
	/// the disposal system is initialized.
	///
	/// The first value of each *type* to be deferred will modify the disposal
	/// system to handle its type.
	///
	/// All subsequent drops of a type that has been deferred before will happen
	/// nearly instantaneously, as they must only observe that the system is set
	/// up for them, and move the value into the transfer queue.
	///
	/// # Usage
	///
	/// ```rust
	/// use wyz::wm::BgDropExt;
	///
	/// vec![1, 2, 3, 4, 5].bg_drop();
	/// vec![6, 7, 8, 9, 10].bg_drop();
	/// ```
	///
	/// If you need to guarantee that your program remains open until all
	/// deferred objects are destroyed, you can block on [`wm::shutdown()`].
	///
	/// [`wm::shutdown()`]: ../fn.shutdown.html
	fn bg_drop(self) -> BgDrop<Self> {
		BgDrop::new(self)
	}
}

impl<T: Sized + 'static> BgDropExt for T {
}

/** Stop the background disposal system.

This function shuts down the disposal system, and ensures that all deferred
destructors in the program are correctly handled. It disables all *future*
deferred-drops from sending values to the worker thread, which forces them to
run their destructors locally. In the meantime, the worker thread will pull all
remaining values out of its work queue and destroy them, then terminate once it
sees that its queue has been closed.

When this function returns, the worker thread will have emptied its queue, torn
down its transfer system, and exited.

You may call this function more than once; it is idempotent. The worker system
is program-global, and will only be started once and stopped once. Once this
function is called, the program will never run deferred disposal again.

Rust does not provide a portable `atexit` behavior, so you are responsible for
calling this before your program terminates if you want to ensure that all
deferred destructions actually take place. Future versions of this library may
register `wm::shutdown()` with the sytem `atexit` handler. If this occurs, the
function will be marked as deprecated on platforms where it is set.
**/
pub fn shutdown() {
	static STOP: Once = Once::new();
	STOP.call_once(|| {
		//  Destroy the sender handle.
		let _: Option<AssertThreadsafe<mpsc::Sender<Dtor>>> = SEND
			.get()
			//  Lock the write guard,
			.and_then(|rw| rw.write().ok())
			//  And remove the sender handle from it.
			.and_then(|mut sender| sender.take());
		//  Close the destructor thread.
		let _: Option<()> = JOIN
			.get()
			//  Lock the thread’s mutex,
			.and_then(|mx| mx.lock().ok())
			//  Remove the handle from it,
			.and_then(|mut mg| mg.take())
			//  And await the thread’s termination.
			.and_then(|jh| jh.join().ok());
	});
}

//  Disposal system implementation

type Dtor = fn() -> ();

//  The sender is never used concurrently.
static SEND: OnceCell<RwLock<Option<AssertThreadsafe<mpsc::Sender<Dtor>>>>> =
	OnceCell::new();
//  The map is only ever used behind a mutex lock.
static DUMP: OnceCell<Mutex<AssertThreadsafe<TypeMap>>> = OnceCell::new();
static JOIN: OnceCell<Mutex<Option<thread::JoinHandle<()>>>> = OnceCell::new();

/// Initialize the collection system.
#[inline(never)]
fn init() {
	let (send, recv) = mpsc::channel::<Dtor>();

	//  Establish a base sending channel. This holds the collector open until
	//  `cancel()` is called.
	SEND.get_or_init(|| {
		send.pipe(AssertThreadsafe::new)
			.pipe(Some)
			.pipe(RwLock::new)
	});

	//  Establish a transfer queue for all types.
	DUMP.get_or_init(|| {
		TypeMap::new().pipe(AssertThreadsafe::new).pipe(Mutex::new)
	});

	//  Start the collector thread.
	JOIN.get_or_init(|| {
		thread::spawn(move || {
			while let Ok(ev) = recv.recv() {
				(ev)()
			}
			let _ = mem::replace(&mut **dq(), TypeMap::new());
		})
		.pipe(Some)
		.pipe(Mutex::new)
	});

	//  TODO(myrrlyn): Register an `atexit` handler to run `shutdown()`.
}

/// Lock the transfer map.
fn dq() -> MutexGuard<'static, AssertThreadsafe<TypeMap>> {
	unsafe { DUMP.get_unchecked() }
		.lock()
		.expect("Collection buffer should never observe a panic")
}

/// Pull the front object out of a typed queue, and destroy it.
fn dtor<T: 'static>() {
	//  Binding a value causes it to drop *after* any temporaries created in its
	//  construction.
	let _tmp = dq()
		//  View the deque containing objects of this type.
		.get_mut::<Key<T>>()
		//  And pop the front value in the queue. It is acceptable to fail.
		.and_then(VecDeque::pop_front);
	//  The mutex lock returned by `dq()` drops immediately after the semicolon,
	//  and the `_tmp` binding drops immediately before the terminating brace.
}

/// Get a local copy of the sender, free of threading concerns.
fn sender() -> Option<mpsc::Sender<Dtor>> {
	//  `sender` is only called after `SEND` is initialized
	unsafe { SEND.get_unchecked() }
		//  Quit if the send channel could not be opened for reading
		.read()
		.ok()?
		//  or if it contains `None`
		.as_ref()?
		.inner
		//  and copy the sender into the local thread.
		.clone()
		.pipe(Some)
}

/// Look up a type’s location in the transfer map.
struct Key<T: 'static>(PhantomData<T>);

impl<T: 'static> typemap::Key for Key<T> {
	/// The transfer map holds some form of collection of the transferred types.
	///
	/// The specific collection type is irrelevant, as long as it supports both
	/// insertion and removal, and has reasonable behavior characteristics.
	/// Since the map has to be completely locked for any transfer event, as the
	/// first transfer of each type must insert its queue into the map, there is
	/// no advantage in trying to make this mpsc-friendly.
	///
	/// If Rust were to allow a construction like
	///
	/// ```rust,ignore
	/// fn type_chan<T: 'static>() -> (
	///   &'static mpsc::Sender<T>,
	///   &'static mpsc::Receiver<T>,
	/// ) {
	///   static MAKE: Once = Once::new();
	///   static mut CHAN: Option<(mpsc::Sender<T>, mpsc::Receiver<T>) = None;
	///   MAKE.call_once(|| unsafe {
	///     CHAN = Some(mpsc::channel());
	///   });
	///   (&CHAN.0, &CHAN.1)
	/// }
	/// ```
	///
	/// then a dynamic type-map would not be necessary at all, since each type
	/// would be granted its own dedicated channel at compile-time. But Rust
	/// does not, so, it is.
	type Value = VecDeque<T>;
}

/** Get off my back, `rustc`.

This is required because `static` vars must be `Sync`, and the thread-safety
wrappers in use apparently inherit the `Sync`hronicity of their wrapped types.
This module uses them correctly, and does not permit escape, so this struct is
needed to get the compiler to accept our use.
**/
#[repr(transparent)]
struct AssertThreadsafe<T> {
	inner: T,
}

impl<T> AssertThreadsafe<T> {
	fn new(inner: T) -> Self {
		Self { inner }
	}
}

unsafe impl<T> Send for AssertThreadsafe<T> {
}

unsafe impl<T> Sync for AssertThreadsafe<T> {
}

impl<T> Deref for AssertThreadsafe<T> {
	type Target = T;

	fn deref(&self) -> &Self::Target {
		&self.inner
	}
}

impl<T> DerefMut for AssertThreadsafe<T> {
	fn deref_mut(&mut self) -> &mut Self::Target {
		&mut self.inner
	}
}

#[cfg(test)]
mod tests {
	use super::*;
	use std::{
		sync::atomic::{
			AtomicUsize,
			Ordering::Relaxed,
		},
		thread,
		time::Duration,
	};

	#[test]
	fn trash_pickup() {
		static COUNTER: AtomicUsize = AtomicUsize::new(0);

		struct Deferrer<F: FnMut()>(F);

		impl<F: FnMut()> Drop for Deferrer<F> {
			fn drop(&mut self) {
				(self.0)()
			}
		}

		let kept = Deferrer(|| {
			COUNTER.fetch_add(1, Relaxed);
		});
		let sent = Deferrer(|| {
			COUNTER.fetch_add(1, Relaxed);
		})
		.bg_drop();

		drop(kept);
		drop(sent);

		while COUNTER.load(Relaxed) < 2 {
			thread::sleep(Duration::from_millis(100));
		}
	}
}
