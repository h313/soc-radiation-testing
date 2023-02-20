#include <atomic>
#include <csignal>
#include <cstdlib>
#include <iostream>

#define L1_DASSOC 4
#define L1_SIZE (32 * 1024)
#define L2_DASSOC 8
#define L2_SIZE (2 * 1024 * 1024)

#define L1_USAGE 0.75

size_t memory_size = 0;
uint8_t* memory_space = nullptr;

std::atomic_bool shutdown(false);
std::atomic<uint64_t *> data_location;

void sigint_handler(sig_t s) { shutdown = true; }

inline bool calculate_checksum(uint64_t *addr) {
  // TODO: exercise ALU/multiply-add pipelines and check against known good values
  return true;
}

inline uint8_t *get_random_location() {
  // TODO: figure out a good random shuffle method
  return (memory_space + (memory_size / 2));
}

void read_and_run_crc() {
  size_t l1_access_sz = (L1_SIZE * L1_USAGE / sizeof(uint64_t)) / L1_DASSOC;
  while (true) {
    if (shutdown)
      return;

    data_location = reinterpret_cast<uint64_t *>(get_random_location());

    // Load data into cache, but vertically so we fill it up before testing it :)
    for (int i = 0; i < l1_access_sz; i++) {
      for (int it = 0; it < L1_DASSOC; it++) {
        calculate_checksum(data_location + (it * l1_access_sz) + i);
      }
    }
  }
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    std::cout << "Usage: " << argv[0] << " memory_size" << std::endl;
    return 1;
  }

  // Allocate memory space
  memory_size = 1000 * 1000 * 1000 * std::stoll(argv[1]);
  memory_space = static_cast<uint8_t*>(malloc(memory_size));

  std::cout << memory_space << std::endl;

  for (int i = 0; i < memory_size; i++) {
    uint8_t *curr = memory_space + i;
    *curr = 0b10101010;
  }

  signal(SIGINT, reinterpret_cast<void (*)(int)>(sigint_handler));

  read_and_run_crc();

  free(memory_space);

  return 0;
}
