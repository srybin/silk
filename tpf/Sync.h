#pragma once

namespace Parallel {
	class Sync {
	public:
		virtual ~Sync() {
		}

		virtual void Wait() = 0;
		virtual void NotifyAll() = 0;
	};
}