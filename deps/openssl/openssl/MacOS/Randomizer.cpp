/* 
------- Strong random data generation on a Macintosh (pre - OS X) ------
		
--	GENERAL: We aim to generate unpredictable bits without explicit
	user interaction. A general review of the problem may be found
	in RFC 1750, "Randomness Recommendations for Security", and some
	more discussion, of general and Mac-specific issues has appeared
	in "Using and Creating Cryptographic- Quality Random Numbers" by
	Jon Callas (www.merrymeet.com/jon/usingrandom.html).

	The data and entropy estimates provided below are based on my
	limited experimentation and estimates, rather than by any
	rigorous study, and the entropy estimates tend to be optimistic.
	They should not be considered absolute.

	Some of the information being collected may be correlated in
	subtle ways. That includes mouse positions, timings, and disk
	size measurements. Some obvious correlations will be eliminated
	by the programmer, but other, weaker ones may remain. The
	reliability of the code depends on such correlations being
	poorly understood, both by us and by potential interceptors.

	This package has been planned to be used with OpenSSL, v. 0.9.5.
	It requires the OpenSSL function RAND_add. 

--	OTHER WORK: Some source code and other details have been
	published elsewhere, but I haven't found any to be satisfactory
	for the Mac per se:

	* The Linux random number generator (by Theodore Ts'o, in
	  drivers/char/random.c), is a carefully designed open-source
	  crypto random number package. It collects data from a variety
	  of sources, including mouse, keyboard and other interrupts.
	  One nice feature is that it explicitly estimates the entropy
	  of the data it collects. Some of its features (e.g. interrupt
	  timing) cannot be reliably exported to the Mac without using
	  undocumented APIs.

	* Truerand by Don P. Mitchell and Matt Blaze uses variations
	  between different timing mechanisms on the same system. This
	  has not been tested on the Mac, but requires preemptive
	  multitasking, and is hardware-dependent, and can't be relied
	  on to work well if only one oscillator is present.

	* Cryptlib's RNG for the Mac (RNDMAC.C by Peter Gutmann),
	  gathers a lot of information about the machine and system
	  environment. Unfortunately, much of it is constant from one
	  startup to the next. In other words, the random seed could be
	  the same from one day to the next. Some of the APIs are
	  hardware-dependent, and not all are compatible with Carbon (OS
	  X). Incidentally, the EGD library is based on the UNIX entropy
	  gathering methods in cryptlib, and isn't suitable for MacOS
	  either.

	* Mozilla (and perhaps earlier versions of Netscape) uses the
	  time of day (in seconds) and an uninitialized local variable
	  to seed the random number generator. The time of day is known
	  to an outside interceptor (to within the accuracy of the
	  system clock). The uninitialized variable could easily be
	  identical between subsequent launches of an application, if it
	  is reached through the same path.

	* OpenSSL provides the function RAND_screen(), by G. van
	  Oosten, which hashes the contents of the screen to generate a
	  seed. This is not useful for an extension or for an
	  application which launches at startup time, since the screen
	  is likely to look identical from one launch to the next. This
	  method is also rather slow.

	* Using variations in disk drive seek times has been proposed
	  (Davis, Ihaka and Fenstermacher, world.std.com/~dtd/;
	  Jakobsson, Shriver, Hillyer and Juels,
	  www.bell-labs.com/user/shriver/random.html). These variations
	  appear to be due to air turbulence inside the disk drive
	  mechanism, and are very strongly unpredictable. Unfortunately
	  this technique is slow, and some implementations of it may be
	  patented (see Shriver's page above.) It of course cannot be
	  used with a RAM disk.

--	TIMING: On the 601 PowerPC the time base register is guaranteed
	to change at least once every 10 addi instructions, i.e. 10
	cycles. On a 60 MHz machine (slowest PowerPC) this translates to
	a resolution of 1/6 usec. Newer machines seem to be using a 10
	cycle resolution as well.
	
	For 68K Macs, the Microseconds() call may be used. See Develop
	issue 29 on the Apple developer site
	(developer.apple.com/dev/techsupport/develop/issue29/minow.html)
	for information on its accuracy and resolution. The code below
	has been tested only on PowerPC based machines.

	The time from machine startup to the launch of an application in
	the startup folder has a variance of about 1.6 msec on a new G4
	machine with a defragmented and optimized disk, most extensions
	off and no icons on the desktop. This can be reasonably taken as
	a lower bound on the variance. Most of this variation is likely
	due to disk seek time variability. The distribution of startup
	times is probably not entirely even or uncorrelated. This needs
	to be investigated, but I am guessing that it not a majpor
	problem. Entropy = log2 (1600/0.166) ~= 13 bits on a 60 MHz
	machine, ~16 bits for a 450 MHz machine.

	User-launched application startup times will have a variance of
	a second or more relative to machine startup time. Entropy >~22
	bits.

	Machine startup time is available with a 1-second resolution. It
	is predictable to no better a minute or two, in the case of
	people who show up punctually to work at the same time and
	immediately start their computer. Using the scheduled startup
	feature (when available) will cause the machine to start up at
	the same time every day, making the value predictable. Entropy
	>~7 bits, or 0 bits with scheduled startup.

	The time of day is of course known to an outsider and thus has 0
	entropy if the system clock is regularly calibrated.

--	KEY TIMING: A  very fast typist (120 wpm) will have a typical
	inter-key timing interval of 100 msec. We can assume a variance
	of no less than 2 msec -- maybe. Do good typists have a constant
	rhythm, like drummers? Since what we measure is not the
	key-generated interrupt but the time at which the key event was
	taken off the event queue, our resolution is roughly the time
	between process switches, at best 1 tick (17 msec). I  therefore
	consider this technique questionable and not very useful for
	obtaining high entropy data on the Mac.

--	MOUSE POSITION AND TIMING: The high bits of the mouse position
	are far from arbitrary, since the mouse tends to stay in a few
	limited areas of the screen. I am guessing that the position of
	the mouse is arbitrary within a 6 pixel square. Since the mouse
	stays still for long periods of time, it should be sampled only
	after it was moved, to avoid correlated data. This gives an
	entropy of log2(6*6) ~= 5 bits per measurement.

	The time during which the mouse stays still can vary from zero
	to, say, 5 seconds (occasionally longer). If the still time is
	measured by sampling the mouse during null events, and null
	events are received once per tick, its resolution is 1/60th of a
	second, giving an entropy of log2 (60*5) ~= 8 bits per
	measurement. Since the distribution of still times is uneven,
	this estimate is on the high side.

	For simplicity and compatibility across system versions, the
	mouse is to be sampled explicitly (e.g. in the event loop),
	rather than in a time manager task.

--	STARTUP DISK TOTAL FILE SIZE: Varies typically by at least 20k
	from one startup to the next, with 'minimal' computer use. Won't
	vary at all if machine is started again immediately after
	startup (unless virtual memory is on), but any application which
	uses the web and caches information to disk is likely to cause
	this much variation or more. The variation is probably not
	random, but I don't know in what way. File sizes tend to be
	divisible by 4 bytes since file format fields are often
	long-aligned. Entropy > log2 (20000/4) ~= 12 bits.
	
--	STARTUP DISK FIRST AVAILABLE ALLOCATION BLOCK: As the volume
	gets fragmented this could be anywhere in principle. In a
	perfectly unfragmented volume this will be strongly correlated
	with the total file size on the disk. With more fragmentation
	comes less certainty. I took the variation in this value to be
	1/8 of the total file size on the volume.

--	SYSTEM REQUIREMENTS: The code here requires System 7.0 and above
	(for Gestalt and Microseconds calls). All the calls used are
	Carbon-compatible.
*/

/*------------------------------ Includes ----------------------------*/

#include "Randomizer.h"

// Mac OS API
#include <Files.h>
#include <Folders.h>
#include <Events.h>
#include <Processes.h>
#include <Gestalt.h>
#include <Resources.h>
#include <LowMem.h>

// Standard C library
#include <stdlib.h>
#include <math.h>

/*---------------------- Function declarations -----------------------*/

// declared in OpenSSL/crypto/rand/rand.h
extern "C" void RAND_add (const void *buf, int num, double entropy);

unsigned long GetPPCTimer (bool is601);	// Make it global if needed
					// elsewhere

/*---------------------------- Constants -----------------------------*/

#define kMouseResolution 6		// Mouse position has to differ
					// from the last one by this
					// much to be entered
#define kMousePositionEntropy 5.16	// log2 (kMouseResolution**2)
#define kTypicalMouseIdleTicks 300.0	// I am guessing that a typical
					// amount of time between mouse
					// moves is 5 seconds
#define kVolumeBytesEntropy 12.0	// about log2 (20000/4),
					// assuming a variation of 20K
					// in total file size and
					// long-aligned file formats.
#define kApplicationUpTimeEntropy 6.0	// Variance > 1 second, uptime
					// in ticks  
#define kSysStartupEntropy 7.0		// Entropy for machine startup
					// time


/*------------------------ Function definitions ----------------------*/

CRandomizer::CRandomizer (void)
{
	long	result;
	
	mSupportsLargeVolumes =
		(Gestalt(gestaltFSAttr, &result) == noErr) &&
		((result & (1L << gestaltFSSupports2TBVols)) != 0);
	
	if (Gestalt (gestaltNativeCPUtype, &result) != noErr)
	{
		mIsPowerPC = false;
		mIs601 = false;
	}
	else
	{
		mIs601 = (result == gestaltCPU601);
		mIsPowerPC = (result >= gestaltCPU601);
	}
	mLastMouse.h = mLastMouse.v = -10;	// First mouse will
						// always be recorded
	mLastPeriodicTicks = TickCount();
	GetTimeBaseResolution ();
	
	// Add initial entropy
	AddTimeSinceMachineStartup ();
	AddAbsoluteSystemStartupTime ();
	AddStartupVolumeInfo ();
	AddFiller ();
}

void CRandomizer::PeriodicAction (void)
{
	AddCurrentMouse ();
	AddNow (0.0);	// Should have a better entropy estimate here
	mLastPeriodicTicks = TickCount();
}

/*------------------------- Private Methods --------------------------*/

void CRandomizer::AddCurrentMouse (void)
{
	Point mouseLoc;
	unsigned long lastCheck;	// Ticks since mouse was last
					// sampled

#if TARGET_API_MAC_CARBON
	GetGlobalMouse (&mouseLoc);
#else
	mouseLoc = LMGetMouseLocation();
#endif
	
	if (labs (mLastMouse.h - mouseLoc.h) > kMouseResolution/2 &&
	    labs (mLastMouse.v - mouseLoc.v) > kMouseResolution/2)
		AddBytes (&mouseLoc, sizeof (mouseLoc),
				kMousePositionEntropy);
	
	if (mLastMouse.h == mouseLoc.h && mLastMouse.v == mouseLoc.v)
		mMouseStill ++;
	else
	{
		double entropy;
		
		// Mouse has moved. Add the number of measurements for
		// which it's been still. If the resolution is too
		// coarse, assume the entropy is 0.

		lastCheck = TickCount() - mLastPeriodicTicks;
		if (lastCheck <= 0)
			lastCheck = 1;
		entropy = log2l
			(kTypicalMouseIdleTicks/(double)lastCheck);
		if (entropy < 0.0)
			entropy = 0.0;
		AddBytes (&mMouseStill, sizeof (mMouseStill), entropy);
		mMouseStill = 0;
	}
	mLastMouse = mouseLoc;
}

void CRandomizer::AddAbsoluteSystemStartupTime (void)
{
	unsigned long	now;		// Time in seconds since
					// 1/1/1904
	GetDateTime (&now);
	now -= TickCount() / 60;	// Time in ticks since machine
					// startup
	AddBytes (&now, sizeof (now), kSysStartupEntropy);
}

void CRandomizer::AddTimeSinceMachineStartup (void)
{
	AddNow (1.5);			// Uncertainty in app startup
					// time is > 1.5 msec (for
					// automated app startup).
}

void CRandomizer::AddAppRunningTime (void)
{
	ProcessSerialNumber PSN;
	ProcessInfoRec		ProcessInfo;
	
	ProcessInfo.processInfoLength = sizeof (ProcessInfoRec);
	ProcessInfo.processName = nil;
	ProcessInfo.processAppSpec = nil;
	
	GetCurrentProcess (&PSN);
	GetProcessInformation (&PSN, &ProcessInfo);

	// Now add the amount of time in ticks that the current process
	// has been active

	AddBytes (&ProcessInfo, sizeof (ProcessInfoRec),
			kApplicationUpTimeEntropy);
}

void CRandomizer::AddStartupVolumeInfo (void)
{
	short			vRefNum;
	long			dirID;
	XVolumeParam	pb;
	OSErr			err;
	
	if (!mSupportsLargeVolumes)
		return;
		
	FindFolder (kOnSystemDisk, kSystemFolderType, kDontCreateFolder,
			&vRefNum, &dirID);
	pb.ioVRefNum = vRefNum;
	pb.ioCompletion = 0;
	pb.ioNamePtr = 0;
	pb.ioVolIndex = 0;
	err = PBXGetVolInfoSync (&pb);
	if (err != noErr)
		return;
		
	// Base the entropy on the amount of space used on the disk and
	// on the next available allocation block. A lot else might be
	// unpredictable, so might as well toss the whole block in. See
	// comments for entropy estimate justifications.

	AddBytes (&pb, sizeof (pb),
		kVolumeBytesEntropy +
		log2l (((pb.ioVTotalBytes.hi - pb.ioVFreeBytes.hi)
				* 4294967296.0D +
			(pb.ioVTotalBytes.lo - pb.ioVFreeBytes.lo))
				/ pb.ioVAlBlkSiz - 3.0));
}

/*
	On a typical startup CRandomizer will come up with about 60
	bits of good, unpredictable data. Assuming no more input will
	be available, we'll need some more lower-quality data to give
	OpenSSL the 128 bits of entropy it desires. AddFiller adds some
	relatively predictable data into the soup.
*/

void CRandomizer::AddFiller (void)
{
	struct
	{
		ProcessSerialNumber psn;	// Front process serial
						// number
		RGBColor	hiliteRGBValue;	// User-selected
						// highlight color
		long		processCount;	// Number of active
						// processes
		long		cpuSpeed;	// Processor speed
		long		totalMemory;	// Total logical memory
						// (incl. virtual one)
		long		systemVersion;	// OS version
		short		resFile;	// Current resource file
	} data;
	
	GetNextProcess ((ProcessSerialNumber*) kNoProcess);
	while (GetNextProcess (&data.psn) == noErr)
		data.processCount++;
	GetFrontProcess (&data.psn);
	LMGetHiliteRGB (&data.hiliteRGBValue);
	Gestalt (gestaltProcClkSpeed, &data.cpuSpeed);
	Gestalt (gestaltLogicalRAMSize, &data.totalMemory);
	Gestalt (gestaltSystemVersion, &data.systemVersion);
	data.resFile = CurResFile ();
	
	// Here we pretend to feed the PRNG completely random data. This
	// is of course false, as much of the above data is predictable
	// by an outsider. At this point we don't have any more
	// randomness to add, but with OpenSSL we must have a 128 bit
	// seed before we can start. We just add what we can, without a
	// real entropy estimate, and hope for the best.

	AddBytes (&data, sizeof(data), 8.0 * sizeof(data));
	AddCurrentMouse ();
	AddNow (1.0);
}

//-------------------  LOW LEVEL ---------------------

void CRandomizer::AddBytes (void *data, long size, double entropy)
{
	RAND_add (data, size, entropy * 0.125);	// Convert entropy bits
						// to bytes
}

void CRandomizer::AddNow (double millisecondUncertainty)
{
	long time = SysTimer();
	AddBytes (&time, sizeof (time), log2l (millisecondUncertainty *
			mTimebaseTicksPerMillisec));
}

//----------------- TIMING SUPPORT ------------------

void CRandomizer::GetTimeBaseResolution (void)
{	
#ifdef __powerc
	long speed;
	
	// gestaltProcClkSpeed available on System 7.5.2 and above
	if (Gestalt (gestaltProcClkSpeed, &speed) != noErr)
		// Only PowerPCs running pre-7.5.2 are 60-80 MHz
		// machines.
		mTimebaseTicksPerMillisec =  6000.0D;
	// Assume 10 cycles per clock update, as in 601 spec. Seems true
	// for later chips as well.
	mTimebaseTicksPerMillisec = speed / 1.0e4D;
#else
	// 68K VIA-based machines (see Develop Magazine no. 29)
	mTimebaseTicksPerMillisec = 783.360D;
#endif
}

unsigned long CRandomizer::SysTimer (void)	// returns the lower 32
						// bit of the chip timer
{
#ifdef __powerc
	return GetPPCTimer (mIs601);
#else
	UnsignedWide usec;
	Microseconds (&usec);
	return usec.lo;
#endif
}

#ifdef __powerc
// The timebase is available through mfspr on 601, mftb on later chips.
// Motorola recommends that an 601 implementation map mftb to mfspr
// through an exception, but I haven't tested to see if MacOS actually
// does this. We only sample the lower 32 bits of the timer (i.e. a
// few minutes of resolution)

asm unsigned long GetPPCTimer (register bool is601)
{
	cmplwi	is601, 0	// Check if 601
	bne	_601		// if non-zero goto _601
	mftb  	r3		// Available on 603 and later.
	blr			// return with result in r3
_601:
	mfspr r3, spr5  	// Available on 601 only.
				// blr inserted automatically
}
#endif
