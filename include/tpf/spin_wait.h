#pragma once

#include <thread>

namespace tpf {
    class spin_wait {
    public:
        spin_wait() : iteration_(1), nanoseconds_(50) {
        }

        void spin_once() {
            if (iteration_ <= 10 && std::thread::hardware_concurrency() > 1) {
                std::chrono::steady_clock::time_point start = std::chrono::high_resolution_clock::now();
                std::chrono::nanoseconds nanoseconds = std::chrono::nanoseconds(nanoseconds_);

                while (std::chrono::high_resolution_clock::now() - start < nanoseconds) {};

                nanoseconds_ = nanoseconds_ * 2;
            } else {
                int num = iteration_ - 10;

                if (num % 20 == 19) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                } else if (num % 5 == 4) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(0));
                } else {
                    std::this_thread::yield();
                }
            }

            iteration_++;
        }

    private:
        int iteration_;
        int nanoseconds_;
    };
}
