#pragma once
#include "windows.h"
#include "Sync.h"

namespace Parallel {
	class SyncBasedOnWindowsEvent : public Sync {
	public:
		SyncBasedOnWindowsEvent();

		~SyncBasedOnWindowsEvent();

		void Wait() override;

		void NotifyAll() override;

	private:
		HANDLE _handle;
	};
}