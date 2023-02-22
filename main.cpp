#include <atomic>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <random>

#define L1_DASSOC 4
#define L1_SIZE (32 * 1024)
#define L2_DASSOC 8
#define L2_SIZE (2 * 1024 * 1024)

#define L1_USAGE 0.75

#define RANDOM_SEED 2020

enum thread_state {
  INIT,
  WAITING,
  READY,
  TEST,
  COMPARE,
  COMPLETE
};

size_t memory_size = 0;
size_t l1_access_sz = (L1_SIZE * L1_USAGE / sizeof(uint64_t)) / L1_DASSOC;
uint8_t* memory_space = nullptr;

std::ranlux48_base *ranlux48;
std::uniform_int_distribution<size_t> *uniform_dist;

std::atomic_bool shutdown(false);
std::atomic<uint64_t *> data_location;
std::atomic<thread_state> td_state[2];

void sigint_handler(int s) { shutdown = true; }

inline bool test_addr(uint64_t *addr) {
  // Read data from memory
  uint64_t test_val = *addr;
  uint64_t test_result_1, test_result_2;

  std::cout << "Read value:" << std::hex << test_val << std::endl;

  // Exercise ALU pipeline
  test_result_1 = (test_val >> 16) + 25;

  // Check against known good value
  std::cout << "ALU value:" << std::hex << test_result_1 << std::endl;

  // Exercise multiply-add pipeline
  test_result_2 = (test_val * 7) + 25;

  // Check against known good value
  std::cout << "Multiply-Add value:" << std::hex << test_result_2 << std::endl;

  return true;
}

inline void get_random_location() {
  // TODO: figure out a good deterministic random shuffle method
  size_t test_location = (*uniform_dist)(*ranlux48);
  data_location = (uint64_t *) (memory_space + test_location);
}

void read_and_run_crc(size_t td) {
  int i = 0, it = 0;

  while (true) {
    if (shutdown)
      return;

    if (td == 0)
      get_random_location();

    td_state[td] = READY;

    while (td_state[0] != READY) {}

    // Load data into cache, but vertically so we fill it up before testing it :)
    for (i = 0; i < l1_access_sz; i++) {
      for (it = 0; it < L1_DASSOC; it++) {
        td_state[td] = TEST;
        test_addr(data_location + (it * l1_access_sz) + i);
        td_state[td] = COMPARE;
      }
    }

    td_state[td] = WAITING;

    // Now that data is in cache, we can go through it linearly
    for (i = 0; i < L1_SIZE * L1_USAGE / sizeof(uint64_t); i++) {
      td_state[td] = TEST;
      test_addr(data_location + i);
      td_state[td] = COMPARE;
    }

    td_state[td] = COMPLETE;
  }
}

int main(int argc, char *argv[]) {
  uint8_t *curr = nullptr;
  size_t i = 0;

  if (argc != 2) {
    std::cout << "Usage: " << argv[0] << " memory_size" << std::endl;
    return 1;
  }

  td_state[0] = INIT;
  td_state[1] = INIT;

  // Allocate memory space
  memory_size = 1000 * 1000 * std::atoll(argv[1]);
  memory_space = (uint8_t*) malloc(memory_size);

  // Set up random number generator
  ranlux48 = new std::ranlux48_base(RANDOM_SEED);
  uniform_dist = new std::uniform_int_distribution<size_t>(0, memory_size / sizeof(uint64_t) - l1_access_sz);

  std::cout << memory_space << std::endl;

  for (i = 0; i < memory_size; i++) {
    curr = memory_space + i;
    *curr = 0b10101010;
  }

  signal(SIGINT, sigint_handler);

  read_and_run_crc(0);

  delete ranlux48;
  delete uniform_dist;

  free(memory_space);

  return 0;
}
