#include <winsock2.h>
#include "Win32IO.h"
#include "IO.h"
#include "windows.h"
#include "Scheduler.h"

using namespace Parallel;

IoQueueBasedOnWindowsCompletionPorts::IoQueueBasedOnWindowsCompletionPorts() : _handle(CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0)) {
}

IoQueueBasedOnWindowsCompletionPorts::~IoQueueBasedOnWindowsCompletionPorts() {
	CloseHandle(_handle);
}

void IoQueueBasedOnWindowsCompletionPorts::Enqueue(void* io, CompletionContainer* completion) {
	CreateIoCompletionPort((HANDLE)io, _handle, (ULONG_PTR)completion, 0);
}

bool IoQueueBasedOnWindowsCompletionPorts::TryDequeue(CompletionContainer& completion) {
	DWORD bytesCopied = 0;
	ULONG_PTR completionKey = 0;
	OVERLAPPED* overlapped = nullptr;

	if (GetQueuedCompletionStatus(_handle, &bytesCopied, &completionKey, &overlapped, INFINITE)) {
		if (bytesCopied == 0 && completionKey == 0 && overlapped == nullptr) {
			return false;
		}

		completion = *reinterpret_cast<CompletionContainer*>(completionKey);
		return true;
	}

	return false;
}

void Parallel::ReadFileAsync(char* path, void* buffer, int bufferSize, int offset, Task* continuation) {
	HANDLE f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
	Scheduler::Instance()->EnqueueInIoQueue(f, continuation);
	OVERLAPPED o = {};
	o.Offset = offset;
	ReadFile(f, buffer, bufferSize, NULL, &o);
	CloseHandle(f);
}

void Parallel::WriteFileAsync(char* path, void* buffer, int bufferSize, Task* continuation) {
	HANDLE f = CreateFileA(path, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_FLAG_OVERLAPPED, NULL);
	Scheduler::Instance()->EnqueueInIoQueue(f, continuation);
	OVERLAPPED o = {};
	WriteFile(f, buffer, bufferSize, NULL, &o);
	CloseHandle(f);
}

void Parallel::AppendToFileAsync(char* path, void* buffer, int bufferSize, Task* continuation) {
	HANDLE f = CreateFileA(path, FILE_APPEND_DATA, 0, 0, OPEN_ALWAYS, FILE_FLAG_OVERLAPPED, NULL);
	Scheduler::Instance()->EnqueueInIoQueue(f, continuation);
	OVERLAPPED o = {};
	WriteFile(f, buffer, bufferSize, NULL, &o);
	CloseHandle(f);
}

void Parallel::ReceiveAsync(void* socket, void* buffer, int bufferSize, Task* continuation) {
	Scheduler::Instance()->EnqueueInIoQueue(socket, continuation);
	WSABUF wsabuf;
	wsabuf.len = bufferSize;
	wsabuf.buf = (char*)buffer;
	OVERLAPPED o = {};
	DWORD dwFlags = 0, dwBytes = 0;
	WSARecv((SOCKET)socket, &wsabuf, 1, &dwBytes, &dwFlags, &o, NULL);
}

void Parallel::SendAsync(void* socket, void* buffer, int bufferSize, Task* continuation) {
	Scheduler::Instance()->EnqueueInIoQueue(socket, continuation);
	WSABUF wsabuf;
	wsabuf.len = bufferSize;
	wsabuf.buf = (char*)buffer;
	OVERLAPPED o = {};
	DWORD dwFlags = 0, dwBytes = 0;
	WSASend((SOCKET)socket, &wsabuf, 1, &dwBytes, dwFlags, &o, NULL);
}