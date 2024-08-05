#include "rw_lock.h"

static bool native_rw_locks_supported = false;
static bool module_load_attempted = false;
static HMODULE library = NULL;

RWLockWrapper *RWLockWrapper::Create()
{
	if (!LoadModule())
	{
		return NULL;
	}
	return new RWLockWrapper();
}

bool RWLockWrapper::LoadModule()
{
	return true;
}

RWLockWrapper::RWLockWrapper()
{
	InitializeSRWLock(&lock_);
}

void RWLockWrapper::AcquireLockExclusive()
{
	AcquireSRWLockExclusive(&lock_);
}
void RWLockWrapper::ReleaseLockExclusive()
{
	ReleaseSRWLockExclusive(&lock_);
}

void RWLockWrapper::AcquireLockShared()
{
	AcquireSRWLockShared(&lock_);
}
void RWLockWrapper::ReleaseLockShared()
{
	ReleaseSRWLockShared(&lock_);
}