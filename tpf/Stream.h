#pragma once
#include "Task.h"
#include "IO.h"
#include <rpcndr.h>

namespace Parallel {
	class CopyToFileFromSocketTask : public Parallel::Task {
	public:
		CopyToFileFromSocketTask(void* socket, char* file, int size)
			: _state(0), _size(size), _buffer(new byte[size]), _socket(socket), _file(file) {
		}

		Task* Compute() override {
			if (_state == 0) {
				Recycle();
				_state = 1;
				memset(_buffer, 0, _size);
				Parallel::ReceiveAsync(_socket, _buffer, _size, this);
				return nullptr;
			}

			if (_buffer[0] == 0) {
				return nullptr;
			}

			Recycle();
			_state = 0;
			Parallel::AppendToFileAsync(_file, _buffer, _size, this);
			return nullptr;
		}

	private:
		int _state;
		int _size;
		byte* _buffer;
		void* _socket;
		char* _file;
	};

	class CopyToSocketFromFileTask : public Parallel::Task {
	public:
		CopyToSocketFromFileTask(char* file, void* socket, int size)
			: _state(0), _size(size), _offset(-_size), _buffer(new byte[size]), _socket(socket), _file(file) {
		}

		Task* Compute() override {
			if (_state == 0) {
				Recycle();
				_state = 1;
				memset(_buffer, 0, _size);
				_offset += _size;
				Parallel::ReadFileAsync(_file, _buffer, _size, _offset, this);
				return nullptr;
			}

			if (_buffer[0] == 0) {
				return nullptr;
			}

			Recycle();
			_state = 0;
			Parallel::SendAsync(_socket, _buffer, _size, this);
			return nullptr;
		}

	private:
		int _state;
		int _size;
		int _offset;
		byte* _buffer;
		void* _socket;
		char* _file;
	};
}