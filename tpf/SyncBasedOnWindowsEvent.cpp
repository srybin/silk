#include "SyncBasedOnWindowsEvent.h"

using namespace Parallel;

SyncBasedOnWindowsEvent::SyncBasedOnWindowsEvent() : _handle(CreateEvent(NULL, true, false, NULL)) {
}

SyncBasedOnWindowsEvent::~SyncBasedOnWindowsEvent() {
	CloseHandle(_handle);
}

void SyncBasedOnWindowsEvent::Wait() {
	WaitForSingleObject(_handle, INFINITE);
}

void SyncBasedOnWindowsEvent::NotifyAll() {
	SetEvent(_handle);
	ResetEvent(_handle);
}