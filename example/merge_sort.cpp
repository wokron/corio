#include <algorithm>
#include <corio.hpp>
#include <iostream>

template <typename T>
corio::Lazy<void> merge_sort(T arr[], std::size_t size, T tmp[]) {
    if (size <= 1) {
        co_return;
    }
    std::size_t mid = size / 2;

    auto t = co_await corio::spawn(merge_sort(arr, mid, tmp));
    co_await merge_sort(arr + mid, size - mid, tmp + mid);
    co_await t;

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

int main() {
    constexpr int SIZE = 100000;

    int arr[SIZE];
    generate_random_array(arr, SIZE);

    int tmp[SIZE];
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