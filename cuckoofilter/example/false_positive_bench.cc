#include "cuckoofilter.h"
#include <libcuckoo/cuckoohash_map.hh>

#include <assert.h>
#include <math.h>
#include <sys/time.h>

#include <cstdlib>
#include <iostream>
#include <vector>
#include <thread>
#include <fstream>

using cuckoofilter::CuckooFilter;

int main(int argc, char **argv) {

  if(argc < 3) {
    std::cout << "Arguments not supplied properly" << std::endl;
    exit(1);
  }

  char* file_name;
  file_name = argv[1];

  // (1<<16) * 2 or (1<<20) * 2
  uint64_t init_size = std::atoll(argv[2]);
  init_size = (1<<16) * 2;
  CuckooFilter<uint64_t, 12> table(init_size);

  std::ifstream file_in;
  std::ofstream file_out;

  file_in.open(file_name);

  std::string op;
  uint64_t key; // val = key
  uint64_t val;

  bool found_in_filter;
  bool found_in_table;

  int num_fasle_pos = 0;
  int num_reads = 0;

  while(file_in >> op) {
    if(op == "READ") {
      std::cout << "READ" << std::endl;
      found_in_filter = table.findinfilter(key);
      found_in_table = table.find(key, val);

      // std::cout << found_in_filter << " " << found_in_table << std::endl;

      if(found_in_filter && !found_in_table)
        num_fasle_pos++;
      num_reads++;

    } else if(op == "INSERT") {
      std::cout << "INSERT" << std::endl;
      table.insert(key, key);
    } else {
      // table.update(key, key);
    }
  }

  std::cout << std::string(file_name) << ", " << num_fasle_pos * 100.0 / num_reads << std::endl;
  std::cout << std::string(file_name) << ", " << num_fasle_pos << " " << num_reads << std::endl;

  // std::cout << "Time taken for lookup: " << elapsed_time_r << std::endl;
  // std::cout << "Time taken for insert: " << elapsed_time_i << std::endl;
  // std::cout << "Time taken for update: " << elapsed_time_u << std::endl;
  // std::cout << "Time taken in total  : " << elapsed_time_u + elapsed_time_i + elapsed_time_r << std::endl;
  // std::cout << "Test Successful: " << std::string(file_name) << std::endl;
  return 0;
}
