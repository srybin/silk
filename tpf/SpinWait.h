#pragma once
#include <thread>

namespace Parallel {
	class SpinWait {
	public:
		SpinWait() : _iteration(1), _nanoseconds(50) {
		}

		void SpinOnce() {
			if (_iteration <= 10 && std::thread::hardware_concurrency() > 1) {
				std::chrono::steady_clock::time_point start = std::chrono::high_resolution_clock::now();
				std::chrono::nanoseconds nanoseconds = std::chrono::nanoseconds(_nanoseconds);

				while (std::chrono::high_resolution_clock::now() - start < nanoseconds);

				_nanoseconds = _nanoseconds * 2;
			} else {
				int num = _iteration - 10;

				if (num % 20 == 19) {
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
				} else if (num % 5 == 4) {
					std::this_thread::sleep_for(std::chrono::milliseconds(0));
				} else {
					std::this_thread::yield();
				}
			}

			_iteration++;
		}

	private:
		int _iteration;
		int _nanoseconds;
	};
}