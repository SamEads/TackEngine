#pragma once

#include <chrono>
#include <unordered_map>

class Profiler {
private:
    class Entry {
    public:
        std::chrono::high_resolution_clock::time_point start;
        std::chrono::high_resolution_clock::time_point end;
    };
public:
    std::unordered_map<std::string, Entry> entries;
    void start(const std::string& e) {
        entries[e].start = std::chrono::high_resolution_clock::now();
    }
    void finish(const std::string& e) {
        entries[e].end = std::chrono::high_resolution_clock::now();
    }
    double getMS(const std::string& e) {
        long long ns = (entries[e].end - entries[e].start).count();
        long double micro = ns / 1'000.0;
        return micro / 1'000.0;
    }
};