// OS 2025 EX1
//
#include <fstream>
#include <iostream>

#include "measure.h"
#include "memory_latency.h"
#include <cstdint>
#include <cstdio>
#include <string>
#define GALOIS_POLYNOMIAL                                                      \
  ((1ULL << 63) | (1ULL << 62) | (1ULL << 60) | (1ULL << 59))

/**
 * Converts the struct timespec to time in nano-seconds.
 * @param t - the struct timespec to convert.
 * @return - the value of time in nano-seconds.
 */
uint64_t nanosectime(struct timespec t) { return t.tv_nsec; }

/**
 * Measures the average latency of accessing a given array in a sequential
 * order.
 * @param repeat - the number of times to repeat the measurement for and average
 * on.
 * @param arr - an allocated (not empty) array to preform measurement on.
 * @param arr_size - the length of the array arr.
 * @param zero - a variable containing zero in a way that the compiler doesn't
 * "know" it in compilation time.
 * @return struct measurement containing the measurement with the following
 * fields: double baseline - the average time (ns) taken to preform the measured
 * operation without memory access. double access_time - the average time (ns)
 * taken to preform the measured operation with memory access. uint64_t rnd -
 * the variable used to randomly access the array, returned to prevent compiler
 * optimizations.
 */
struct measurement measure_sequential_latency(uint64_t repeat,
                                              array_element_t *arr,
                                              uint64_t arr_size,
                                              uint64_t zero) {
  repeat =
      arr_size > repeat ? arr_size : repeat; // Make sure repeat >= arr_size
  // Baseline measurement:
  struct timespec t0;
  timespec_get(&t0, TIME_UTC);   // Baseline Access to memory
  register uint64_t rnd = 12345; // Random num gen
  for (register uint64_t i = 0; i < repeat; i++) {
    register uint64_t index = i % arr_size;
    rnd ^= index & zero;
    rnd =
        (rnd >> 1) ^
        ((0 - (rnd & 1)) &
         GALOIS_POLYNOMIAL); // Advance rnd pseudo-randomly (using Galois LFSR)
  }
  struct timespec t1;
  timespec_get(&t1, TIME_UTC);

  // Memory access measurement:
  struct timespec t2;
  timespec_get(&t2, TIME_UTC);
  rnd = (rnd & zero) ^ 12345;
  for (register uint64_t i = 0; i < repeat; i++) {
    register uint64_t index = i % arr_size;
    rnd ^= arr[index] & zero;
    rnd =
        (rnd >> 1) ^
        ((0 - (rnd & 1)) &
         GALOIS_POLYNOMIAL); // Advance rnd pseudo-randomly (using Galois LFSR)
  }
  struct timespec t3;
  timespec_get(&t3, TIME_UTC);

  // Calculate baseline and memory access times:
  double baseline_per_cycle =
      (double)(nanosectime(t1) - nanosectime(t0)) / (repeat);
  double memory_per_cycle =
      (double)(nanosectime(t3) - nanosectime(t2)) / (repeat);
  struct measurement result;

  result.baseline = baseline_per_cycle;
  result.access_time = memory_per_cycle;
  result.rnd = rnd;
  return result;
}

/**
 * Runs the logic of the memory_latency program. Measures the access latency for
 * random and sequential memory access patterns. Usage: './memory_latency
 * max_size factor repeat' where:
 *      - max_size - the maximum size in bytes of the array to measure access
 * latency for.
 *      - factor - the factor in the geometric series representing the array
 * sizes to check.
 *      - repeat - the number of times each measurement should be repeated for
 * and averaged on. The program will print output to stdout in the following
 * format: mem_size_1,offset_1,offset_sequential_1
 *      mem_size_2,offset_2,offset_sequential_2
 *              ...
 *              ...
 *              ...
 */
int main(int argc, char *argv[]) {
  if (argc != 4) {
    fprintf(stderr, "Usage: max_size factor repeat \n");
    return -1;
  }
  // zero==0, but the compiler doesn't know it. Use as the zero arg of
  // measure_latency and measure_sequential_latency.
  struct timespec t_dummy;
  timespec_get(&t_dummy, TIME_UTC);
  const uint64_t zero =
      nanosectime(t_dummy) > 1000000000ull ? 0 : nanosectime(t_dummy);
  uint64_t max_size = std::stol(argv[1]);
  float factor = std::stod(argv[2]);
  uint64_t repeat = std::stol(argv[3]);

  if (max_size < 100 || factor <= 1 || repeat <= 0) {
    fprintf(stdout,
            "Invalid arguments, maxsize>=100, factor >1 , repeat > 0 \n");
    return -1;
  }
  // The program should run the measurement for arrays of sizes from 100 bytes
  // up to max size
  uint64_t curr_size = 100;
  std::ofstream file("results.csv");
  if (!file.is_open()) {
    std::cerr << "Failed to open file" << std::endl;
    return -1;
  }

  while (curr_size <= max_size) {

    uint64_t num_elements =
        curr_size /
        sizeof(
            array_element_t); // TODO: Ask in Shat kaabala what size is the arr
    array_element_t *arr = (array_element_t *)malloc(curr_size);
    for (uint64_t i = 0; i < num_elements; i++) {
      arr[i] = i;
    }
    struct measurement rnd_measure =
        measure_latency(repeat, arr, num_elements, zero);

    struct measurement seq_measure =
        measure_sequential_latency(repeat, arr, num_elements, zero);

    // calc the offset (access_time - baseline)
    double random_offset = rnd_measure.access_time - rnd_measure.baseline;
    double sequential_offset = seq_measure.access_time - seq_measure.baseline;

    // Print results in the required format
    fprintf(stdout, "%lu,%.2f,%.2f\n", curr_size, random_offset,
            sequential_offset); // TODO : Check in shat kabala how to print
                                // ,include iostream
    file << curr_size << "," << random_offset << "," << sequential_offset
         << std::endl;
    free(arr);
    // Calculate next size in the geometric series
    curr_size = static_cast<uint64_t>(curr_size * factor);
  }
  file.close();

  return 0;
}
