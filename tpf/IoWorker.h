#pragma once
#include "Worker.h"
#include "IO.h"

namespace Parallel {
	class IoWorker : public Worker {
	public:
		IoWorker(IoQueue& ioQueue, Sync* sync);

		void Execute() override;
	private:
		IoQueue& _ioQueue;
	};
}