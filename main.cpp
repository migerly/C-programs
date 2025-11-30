#include <algorithm>
#include <chrono>
#include <execution>
#include <fstream>
#include <iostream>
#include <random>
#include <thread>
#include <vector>


std::vector<int> generate_sorted_vector(size_t n) {
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(0, 1'000'000);

    std::vector<int> v(n);
    for (auto& x : v) x = dist(rng);
    std::sort(v.begin(), v.end());
    return v;
}


template <typename F>
double measure_ms(F&& f, size_t iterations = 1) {
    using namespace std::chrono;

    auto start = high_resolution_clock::now();
    for (size_t i = 0; i < iterations; ++i) f();
    auto end = high_resolution_clock::now();

    double ms = duration_cast<duration<double, std::milli>>(end - start).count();
    return ms / iterations;
}


template <typename It1, typename It2, typename It3>
double benchmark_std_merge_no_policy(It1 a_b, It1 a_e, It2 b_b, It2 b_e, It3 c_b) {
    return measure_ms([&]() {
        std::merge(a_b, a_e, b_b, b_e, c_b);
        });
}


template <typename Exec, typename It1, typename It2, typename It3>
double benchmark_std_merge_policy(Exec&& policy, It1 a_b, It1 a_e, It2 b_b, It2 b_e, It3 c_b) {
    return measure_ms([&]() {
        std::merge(policy, a_b, a_e, b_b, b_e, c_b);
        });
}


void run_std_merge_benchmarks(std::ostream& out, size_t n) {

    out << "=== std::merge benchmarks, N=" << n << " ===\n";

    auto A = generate_sorted_vector(n);
    auto B = generate_sorted_vector(n);
    std::vector<int> C(2 * n);

    double no_pol = benchmark_std_merge_no_policy(A.begin(), A.end(), B.begin(), B.end(), C.begin());
    out << "no policy:    " << no_pol << " ms\n";

    double seq = benchmark_std_merge_policy(std::execution::seq, A.begin(), A.end(), B.begin(), B.end(), C.begin());
    out << "seq:          " << seq << " ms\n";

    double par = benchmark_std_merge_policy(std::execution::par, A.begin(), A.end(), B.begin(), B.end(), C.begin());
    out << "par:          " << par << " ms\n";

    double pun = benchmark_std_merge_policy(std::execution::par_unseq, A.begin(), A.end(), B.begin(), B.end(), C.begin());
    out << "par_unseq:    " << pun << " ms\n";

    out << "\n";
}


double benchmark_custom_parallel_merge(size_t n, size_t K, std::ostream& out) {

    auto A = generate_sorted_vector(n);
    auto B = generate_sorted_vector(n);

    std::vector<std::vector<int>> partial(K);
    std::vector<std::thread> threads(K);

    size_t hw = std::thread::hardware_concurrency();

    size_t chunkA = (A.size() + K - 1) / K;
    size_t chunkB = (B.size() + K - 1) / K;

    using namespace std::chrono;
    auto start = high_resolution_clock::now();

    for (size_t i = 0; i < K; ++i) {
        size_t a_b = std::min(i * chunkA, A.size());
        size_t a_e = std::min((i + 1) * chunkA, A.size());

        size_t b_b = std::min(i * chunkB, B.size());
        size_t b_e = std::min((i + 1) * chunkB, B.size());

        partial[i].resize((a_e - a_b) + (b_e - b_b));

        threads[i] = std::thread([&, i, a_b, a_e, b_b, b_e]() {
            std::merge(A.begin() + a_b, A.begin() + a_e,
                B.begin() + b_b, B.begin() + b_e,
                partial[i].begin());
            });
    }

    for (auto& t : threads) t.join();

    std::vector<int> merged = partial[0];

    for (size_t i = 1; i < K; ++i) {
        std::vector<int> tmp(merged.size() + partial[i].size());
        std::merge(merged.begin(), merged.end(),
            partial[i].begin(), partial[i].end(),
            tmp.begin());
        merged.swap(tmp);
    }

    auto end = high_resolution_clock::now();
    double ms = duration_cast<duration<double, std::milli>>(end - start).count();

    out << "K=" << K << ", time_ms=" << ms << ", hw_threads=" << hw << "\n";
    return ms;
}


void run_custom_parallel_benchmarks(std::ostream& out, size_t n) {

    out << "=== Custom Parallel Merge, N=" << n << " ===\n";
    out << "K\ttime_ms\n";

    size_t bestK = 0;
    double bestTime = 1e100;

    for (size_t K = 1; K <= 16; ++K) {      
        double t = benchmark_custom_parallel_merge(n, K, out);
        if (t < bestTime) {
            bestTime = t;
            bestK = K;
        }
    }

    size_t hw = std::thread::hardware_concurrency();

    out << "\nBEST K = " << bestK
        << ", time = " << bestTime << " ms"
        << ", hw_threads = " << hw
        << ", ratio K/hw = " << double(bestK) / hw << "\n\n";
}


int main() {

    std::ofstream out("merge_results.txt");
    if (!out.is_open()) {
        std::cerr << "Error: cannot open merge_results.txt\n";
        return 1;
    }

    out << "MERGE EXPERIMENTS (Release mode recommended)\n\n";

    std::vector<size_t> sizes = { 100000, 300000, 1000000 };

    for (size_t n : sizes) {
        run_std_merge_benchmarks(out, n);
        run_custom_parallel_benchmarks(out, n);
    }

    return 0;
}
