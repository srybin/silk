#pragma once
#include "worker.h"
#include "tpf/async_io.h"

namespace Parallel {
	class IoWorker : public Worker {
	public:
		IoWorker(IoQueue& ioQueue, Sync* sync);

		void Execute() override;
	private:
		IoQueue& _ioQueue;
	};
}