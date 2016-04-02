#pragma once
#include "Worker.h"

namespace Parallel {
	class Scheduler;

	class CpuWorker : public Worker {
	public:
		CpuWorker(Scheduler* scheduler, Sync* sync);

		void Execute() override;

	private:
		int _waitCount = 0;
		Scheduler* _scheduler;
	};
}