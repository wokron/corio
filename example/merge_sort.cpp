#include <algorithm>
#include <corio.hpp>
#include <iostream>

template <typename T>
corio::Lazy<void> merge_sort(T arr[], std::size_t size, T tmp[],
                             std::size_t concurrency = 1) {
    if (size <= 1) {
        co_return;
    }
    std::size_t mid = size / 2;

    static const std::size_t THRESHOLD = std::thread::hardware_concurrency();
    if (concurrency >= THRESHOLD) {
        co_await merge_sort(arr, mid, tmp, concurrency);
        co_await merge_sort(arr + mid, size - mid, tmp + mid, concurrency);
    } else {
        auto t =
            co_await corio::spawn(merge_sort(arr, mid, tmp, concurrency << 1));
        co_await merge_sort(arr + mid, size - mid, tmp + mid, concurrency << 1);
        co_await t;
    }

    std::size_t i = 0, j = mid, k = 0;
    while (i < mid && j < size) {
        if (arr[i] < arr[j]) {
            tmp[k++] = arr[i++];
        } else {
            tmp[k++] = arr[j++];
        }
    }
    while (i < mid) {
        tmp[k++] = arr[i++];
    }
    while (j < size) {
        tmp[k++] = arr[j++];
    }

    std::copy(tmp, tmp + size, arr);
}

void generate_random_array(int arr[], std::size_t size) {
    for (std::size_t i = 0; i < size; ++i) {
        arr[i] = rand() % size;
    }
}

constexpr int SIZE = 10'000'000;
int arr[SIZE];
int tmp[SIZE];

int main() {
    generate_random_array(arr, SIZE);

    corio::run(merge_sort(arr, SIZE, tmp));

    for (int i = 1; i < SIZE; ++i) {
        if (arr[i - 1] > arr[i]) {
            std::cerr << "Error: merge_sort failed!!!" << std::endl;
            return 1;
        }
    }

    for (int i : arr) {
        std::cout << i << ' ';
    }
    std::cout << std::endl;

    return 0;
}