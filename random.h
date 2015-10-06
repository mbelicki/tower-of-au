#pragma once

#include <random>

class random_t {
    public:
        int uniform_from_range(int min, int max) {
            std::uniform_int_distribution<int> distribution(min, max);
            return distribution(_generator);
        }

        float uniform_zero_to_one() {
            std::uniform_real_distribution<float> distribution(0.0f, 1.0f);
            return distribution(_generator);
        }

    private:
        std::default_random_engine _generator;
};
