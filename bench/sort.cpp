#include <algorithm>
#include <corio.hpp>
#include <marker.hpp>
#include <thread>

template <typename T>
void merge_sort_normal(T arr[], std::size_t size, T tmp[]) {
    if (size <= 1) {
        return;
    }
    std::size_t mid = size / 2;

    merge_sort_normal(arr, mid, tmp);
    merge_sort_normal(arr + mid, size - mid, tmp + mid);

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

template <typename T>
corio::Lazy<void> merge_sort_corio(T arr[], std::size_t size, T tmp[],
                                   std::size_t concurrency = 1) {
    if (size <= 1) {
        co_return;
    }
    std::size_t mid = size / 2;

    static const std::size_t THRESHOLD = std::thread::hardware_concurrency();
    if (concurrency >= THRESHOLD) {
        co_await merge_sort_corio(arr, mid, tmp, concurrency);
        co_await merge_sort_corio(arr + mid, size - mid, tmp + mid,
                                  concurrency);
    } else {
        auto t = co_await corio::spawn(
            merge_sort_corio(arr, mid, tmp, concurrency << 1));
        co_await merge_sort_corio(arr + mid, size - mid, tmp + mid,
                                  concurrency << 1);
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

void launch_merge_sort_normal(int arr[], std::size_t size, int tmp[]) {
    merge_sort_normal(arr, size, tmp);
}

void launch_merge_sort_corio(int arr[], std::size_t size, int tmp[]) {
    corio::run(merge_sort_corio(arr, size, tmp));
}

void generate_random_array(int arr[], std::size_t size) {
    for (std::size_t i = 0; i < size; ++i) {
        arr[i] = rand() % size;
    }
}

constexpr int SIZE = 10'000'000;

int arr[SIZE];
int data[SIZE], tmp[SIZE];

int main() {
    generate_random_array(arr, SIZE);

    for (std::size_t i = 0; i < 6; i++) {
        std::copy(arr, arr + SIZE, data);
        auto dur = marker::measured(launch_merge_sort_corio)(data, SIZE, tmp);
        for (int i = 1; i < SIZE; ++i) {
            if (data[i - 1] > data[i]) {
                std::cerr << "Error: merge_sort_corio failed!!!" << std::endl;
                return 1;
            }
        }
        std::cerr << "merge_sort_corio: " << dur << std::endl;
    }

    for (std::size_t i = 0; i < 6; i++) {
        std::copy(arr, arr + SIZE, data);
        auto dur = marker::measured(launch_merge_sort_normal)(data, SIZE, tmp);
        for (int i = 1; i < SIZE; ++i) {
            if (data[i - 1] > data[i]) {
                std::cerr << "Error: merge_sort_normal failed!!!" << std::endl;
                return 1;
            }
        }
        std::cerr << "merge_sort_normal: " << dur << std::endl;
    }
}