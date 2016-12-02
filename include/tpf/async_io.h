#pragma once
#include "Task.h"
#include <thread>

namespace tpf {
	void ReadFileAsync(char* path, void* buffer, int bufferSize, int offset, Task* continuation);
	void WriteFileAsync(char* path, void* buffer, int bufferSize, Task* continuation);
	void AppendToFileAsync(char* path, void* buffer, int bufferSize, Task* continuation);
	void ReceiveAsync(void* socket, void* buffer, int bufferSize, Task* continuation);
	void SendAsync(void* socket, void* buffer, int bufferSize, Task* continuation);
}