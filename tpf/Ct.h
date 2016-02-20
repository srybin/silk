#pragma once
#include <atomic>

class Ct {
public:
	Ct() : _isCanceled(false) {
	}

	bool IsCanceled(std::memory_order memoryOrder = std::memory_order_acquire) {
		return _isCanceled.load(memoryOrder);
	}

	void Cancel(std::memory_order memoryOrder = std::memory_order_release) {
		_isCanceled.store(true, memoryOrder);
	}

private:
	std::atomic<bool> _isCanceled;
};