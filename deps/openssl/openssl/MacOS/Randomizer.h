
//	Gathers unpredictable system data to be used for generating
//	random bits

#include <MacTypes.h>

class CRandomizer
{
public:
	CRandomizer (void);
	void PeriodicAction (void);
	
private:

	// Private calls

	void		AddTimeSinceMachineStartup (void);
	void		AddAbsoluteSystemStartupTime (void);
	void		AddAppRunningTime (void);
	void		AddStartupVolumeInfo (void);
	void		AddFiller (void);

	void		AddCurrentMouse (void);
	void		AddNow (double millisecondUncertainty);
	void		AddBytes (void *data, long size, double entropy);
	
	void		GetTimeBaseResolution (void);
	unsigned long	SysTimer (void);

	// System Info	
	bool		mSupportsLargeVolumes;
	bool		mIsPowerPC;
	bool		mIs601;
	
	// Time info
	double		mTimebaseTicksPerMillisec;
	unsigned long	mLastPeriodicTicks;
	
	// Mouse info
	long		mSamplePeriod;
	Point		mLastMouse;
	long		mMouseStill;
};
