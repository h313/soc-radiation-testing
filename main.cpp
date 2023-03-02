#include <atomic>
#include <chrono>
#include <csignal>
#include <ctime>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <random>
#include <thread>

#include <sys/wait.h>
#include <pthread.h>

#define L1_DASSOC 4
#define L1_SIZE (32 * 1024)
#define L2_DASSOC 8
#define L2_SIZE (2 * 1024 * 1024)

#define L1_USAGE 0.75

#define RANDOM_SEED 2020

size_t memory_size = 0, l1_access_sz = (L1_SIZE * L1_USAGE / sizeof(uint64_t)) / L1_DASSOC;
uint8_t *memory_space = nullptr;
std::atomic<uint64_t *> data_location;
std::atomic<int> retval[2];

std::ranlux48_base *ranlux48;
std::uniform_int_distribution<size_t> *uniform_dist;

sched_param sch_params;

inline void get_random_location() {
  // TODO: figure out a good deterministic random shuffle method
  size_t test_location = (*uniform_dist)(*ranlux48);
  data_location = (uint64_t *)(memory_space + test_location);
}

void read_and_run_crc(size_t td) {
  int i = 0, it = 0;
  uint64_t *test_val = nullptr;

  pthread_setschedparam(pthread_self(), SCHED_FIFO, &sch_params);

  retval[td] = 0;

  // Load data into cache, but vertically so we fill it up before testing it :)
  for (i = 0; i < l1_access_sz; i++) {
    for (it = 0; it < L1_DASSOC; it++) {
      test_val = data_location + (it * l1_access_sz) + i;
      // Make sure memory data is correct
      if (*test_val != 0xaaaaaaaaaaaaaaaa) {
        std::cout << "td" << td << ": incorrect data read at " << std::hex
                  << ((data_location + (it * l1_access_sz) + i)) << std::endl;
        retval[td] = 1;
      }
    }
  }

  if (retval[td] == 0) {
    // Now that data is in cache, we can go through it linearly
    for (i = 0; i < L1_SIZE * L1_USAGE / sizeof(uint64_t); i++) {
      test_val = data_location + i;

      // Make sure memory data is correct
      if (*test_val != 0xaaaaaaaaaaaaaaaa)
        retval[td] = 2;

      // Exercise ALU pipeline and check against known good value
      else if ((*test_val + 25) >> 11 != 0x15555555555555)
        retval[td] = 3;

      // Exercise multiply-add pipeline and check against known good value
      else if (((*test_val) * (*test_val)) + 25 != 0x38e38e38e38e38fd)
        retval[td] = 4;
    }
  }
}

int main(int argc, char *argv[]) {
  uint8_t *curr = nullptr;
  int timeout;
  size_t i = 0;
  std::ofstream out_file;

  std::thread td_1, td_2;

  if (argc != 4) {
    std::cout << "Usage: " << argv[0] << " memory_size timeout out_file" << std::endl;
    return 1;
  }

  sch_params.sched_priority = 99;
  timeout = std::atoi(argv[2]);
  out_file = std::ofstream(argv[3]);

  // Allocate memory space
  memory_size = 1000 * 1000 * std::atoll(argv[1]);
  memory_space = (uint8_t *)malloc(memory_size);

  // Set up random number generator
  ranlux48 = new std::ranlux48_base(RANDOM_SEED);
  uniform_dist = new std::uniform_int_distribution<size_t>(
      0, memory_size / sizeof(uint64_t) - l1_access_sz);

  // Write alternating 1s and 0s into memory
  for (i = 0; i < memory_size; i++) {
    curr = memory_space + i;
    *curr = 0b10101010;
  }

  std::time_t start_time = std::time(0);

  // Loop through and test on random locations
  while (true) {
    get_random_location();

    td_1 = std::thread(read_and_run_crc, 0);
    td_2 = std::thread(read_and_run_crc, 1);

    td_1.join();
    td_2.join();

    if (retval[0] != 0 && retval[1] != 0)
      out_file << std::time(0) << " " << retval[0] << retval[1] << std::endl;

    if (std::time(0) - start_time >= timeout)
      break;
  }

  delete ranlux48;
  delete uniform_dist;

  out_file.close();
  free(memory_space);

  return 0;
}