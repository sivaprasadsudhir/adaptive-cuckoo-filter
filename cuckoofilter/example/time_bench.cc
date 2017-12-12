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

  double elapsed_time_r = 0.0;
  double elapsed_time_i = 0.0;
  double elapsed_time_u = 0.0;

  struct timeval t_r1, t_r2;
  struct timeval t_i1, t_i2;
  struct timeval t_u1, t_u2;

  while(file_in >> op) {
    if(op == "READ") {
      gettimeofday(&t_r1, NULL);
      table.find(key, val);
      gettimeofday(&t_r2, NULL);
      elapsed_time_r += (t_r2.tv_sec - t_r1.tv_sec) + (t_r2.tv_usec - t_r1.tv_usec) / 1e6;
    } else if(op == "INSERT") {
      gettimeofday(&t_i1, NULL);
      table.insert(key, key);
      gettimeofday(&t_i2, NULL);
      elapsed_time_i += (t_i2.tv_sec - t_i1.tv_sec) + (t_i2.tv_usec - t_i1.tv_usec) / 1e6;
    } else {
      // gettimeofday(&t_u1, NULL);
      // table.update(key, key);
      // gettimeofday(&t_u2, NULL);
      // elapsed_time_u += (t_u2.tv_sec - t_u1.tv_sec) + (t_u2.tv_usec - t_u1.tv_usec) / 1e6;
    }
  }

  std::cout << std::string(file_name) << ", "
            << elapsed_time_r << ", "
            << elapsed_time_i << ", "
            << elapsed_time_u << ", "
            << elapsed_time_u + elapsed_time_i + elapsed_time_r << std::endl;

  // std::cout << "Time taken for lookup: " << elapsed_time_r << std::endl;
  // std::cout << "Time taken for insert: " << elapsed_time_i << std::endl;
  // std::cout << "Time taken for update: " << elapsed_time_u << std::endl;
  // std::cout << "Time taken in total  : " << elapsed_time_u + elapsed_time_i + elapsed_time_r << std::endl;
  // std::cout << "Test Successful: " << std::string(file_name) << std::endl;
  return 0;
}
