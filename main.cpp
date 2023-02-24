#include <atomic>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <random>
#include <sys/wait.h>
#include <thread>

#include <pthread.h>

#define L1_DASSOC 4
#define L1_SIZE (32 * 1024)
#define L2_DASSOC 8
#define L2_SIZE (2 * 1024 * 1024)

#define L1_USAGE 0.75

#define RANDOM_SEED 2020

size_t memory_size = 0;
size_t l1_access_sz = (L1_SIZE * L1_USAGE / sizeof(uint64_t)) / L1_DASSOC;
uint8_t *memory_space = nullptr;

std::ranlux48_base *ranlux48;
std::uniform_int_distribution<size_t> *uniform_dist;

std::atomic_bool shutdown(false);
std::atomic<uint64_t *> data_location;

void sigint_handler(int s) { shutdown = true; }

inline void get_random_location() {
  // TODO: figure out a good deterministic random shuffle method
  size_t test_location = (*uniform_dist)(*ranlux48);
  data_location = (uint64_t *)(memory_space + test_location);
}

void read_and_run_crc(size_t td) {
  int i = 0, it = 0;
  uint64_t *test_val;

  // Load data into cache, but vertically so we fill it up before testing it :)
  for (i = 0; i < l1_access_sz; i++) {
    for (it = 0; it < L1_DASSOC; it++) {
      test_val = data_location + (it * l1_access_sz) + i;
      // Make sure memory data is correct
      if (*test_val != 0xaaaaaaaaaaaaaaaa)
        std::cout << "td" << td << ": incorrect data read at " << std::hex
                  << ((data_location + (it * l1_access_sz) + i)) << std::endl;
    }
  }

  // Now that data is in cache, we can go through it linearly
  for (i = 0; i < L1_SIZE * L1_USAGE / sizeof(uint64_t); i++) {
    test_val = data_location + i;
    // Make sure memory data is correct
    if (*test_val != 0xaaaaaaaaaaaaaaaa)
      std::cout << "td" << td << ": incorrect cache read at " << std::hex
                << ((data_location + (it * l1_access_sz) + i)) << std::endl;

    // Exercise ALU pipeline and check against known good value
    if ((*test_val + 25) >> 11 != 0x15555555555555)
      std::cout << "td" << td << ": incorrect ALU result at " << std::hex
                << ((data_location + (it * l1_access_sz) + i)) << std::endl;

    // Exercise multiply-add pipeline and check against known good value
    if (((*test_val) * (*test_val)) + 25 != 0x38e38e38e38e38fd)
      std::cout << "td" << td << ": incorrect multiply-add result at "
                << std::hex << ((data_location + (it * l1_access_sz) + i))
                << std::endl;
  }
}

int main(int argc, char *argv[]) {
  uint8_t *curr = nullptr;
  size_t i = 0;
  sched_param sch_params;

  if (argc != 2) {
    std::cout << "Usage: " << argv[0] << " memory_size" << std::endl;
    return 1;
  }

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

  sch_params.sched_priority = 99;
  signal(SIGINT, sigint_handler);

  // Loop through and test on random locations
  while (true) {
    if (shutdown)
      break;

    get_random_location();

    std::thread td_1(read_and_run_crc, 0);
    std::thread td_2(read_and_run_crc, 1);

    pthread_setschedparam(td_1.native_handle(), SCHED_FIFO, &sch_params);
    pthread_setschedparam(td_2.native_handle(), SCHED_FIFO, &sch_params);

    td_1.join();
    td_2.join();
  }

  delete ranlux48;
  delete uniform_dist;

  free(memory_space);

  return 0;
}