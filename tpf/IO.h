#pragma once
#include "Task.h"
#include <thread>

namespace Parallel {
	struct CompletionContainer {
		Task* Continuation;
		std::thread::id ThreadId;

		CompletionContainer() {
		}

		CompletionContainer(Task* continuation, std::thread::id threadId) : Continuation(continuation), ThreadId(threadId) {
		}
	};

	class IoQueue {
	public:
		virtual ~IoQueue() {
		}

		void virtual Enqueue(void* io, CompletionContainer* completion) = 0;

		bool virtual TryDequeue(CompletionContainer& completion) = 0;
	};

	void ReadFileAsync(char* path, void* buffer, int bufferSize, int offset, Task* continuation);
	void WriteFileAsync(char* path, void* buffer, int bufferSize, Task* continuation);
	void AppendToFileAsync(char* path, void* buffer, int bufferSize, Task* continuation);
	void ReceiveAsync(void* socket, void* buffer, int bufferSize, Task* continuation);
	void SendAsync(void* socket, void* buffer, int bufferSize, Task* continuation);
}