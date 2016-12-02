#pragma once
#include "tpf/async_io.h"
#include "windows.h"

namespace Parallel {
	class IoQueueBasedOnWindowsCompletionPorts : public IoQueue {
	public:
		IoQueueBasedOnWindowsCompletionPorts();

		virtual ~IoQueueBasedOnWindowsCompletionPorts();

		void Enqueue(void* io, CompletionContainer* completion) override;

		bool TryDequeue(CompletionContainer& completion) override;
	private:
		HANDLE _handle;
	};
}