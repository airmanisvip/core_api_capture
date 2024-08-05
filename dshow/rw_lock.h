#pragma once

#include <Windows.h>

class RWLockWrapper
{
public:
	static RWLockWrapper *Create();

	void AcquireLockExclusive();
	void ReleaseLockExclusive();

	void AcquireLockShared();
	void ReleaseLockShared();

private:
	RWLockWrapper();
	static bool LoadModule();

	SRWLOCK lock_;
};

class ReadLockScoped
{
public:
	explicit ReadLockScoped(RWLockWrapper& rw_lock)
		: rw_lock_(rw_lock)
	{
		rw_lock_.AcquireLockShared();
	}

	~ReadLockScoped() 
	{
		rw_lock_.ReleaseLockShared();
	}

private:
	RWLockWrapper& rw_lock_;
};

class WriteLockScoped
{
public:
	explicit WriteLockScoped(RWLockWrapper& rw_lock)
		: rw_lock_(rw_lock)
	{
		rw_lock_.AcquireLockExclusive();
	}

	~WriteLockScoped()
	{
		rw_lock_.ReleaseLockExclusive();
	}

private:
	RWLockWrapper& rw_lock_;
};