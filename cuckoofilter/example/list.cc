#include "cuckoofilter.h"
#include <libcuckoo/cuckoohash_map.hh>

#include <assert.h>
#include <math.h>
#include <sys/time.h>

#include <cstdlib>
#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <fstream>

#define NUM_LISTS 10

using cuckoofilter::CuckooFilter;

char *file_name;

uint64_t init_size;
int num_threads;
double elapsed_time_r = 0.0;
double elapsed_time_i = 0.0;
double elapsed_time_u = 0.0;
uint64_t num_inserts = 0;


class FilterList {
  uint64_t num_elements;
  int next_list;
  bool could_not_insert = false;
  CuckooFilter<uint64_t, 12> *tables[NUM_LISTS];

public:
  FilterList(uint64_t num_e  = ((1 << 16)*2)) {
    for(int i = 0; i < NUM_LISTS; i++) {
      tables[i] = new CuckooFilter<uint64_t, 12>(num_e); 
    }
    num_elements = num_e;
    next_list = 0;
  }


  bool insert(uint64_t key, uint64_t val) {
    if(could_not_insert) {
      return false;
    }
    for(int i = 0; i < NUM_LISTS; i++) {
      int insert_at = (next_list++)%NUM_LISTS;
      next_list = insert_at;
      if(tables[insert_at]->insert(key, val)) {
        return true;
      }
    }
    could_not_insert = true;
    return false;
  }

  bool find(uint64_t key, uint64_t &val) {
    for(int i = 0; i < NUM_LISTS; i++) {
      if(tables[i]->find(key, val)) {
        return true;
      }
    }
    return false;
  }
};


FilterList *table_list;

int main(int argc, char **argv) {

  if(argc < 3) {
    std::cout << "Arguments not supplied properly" << std::endl;
    exit(1);
  }

  std::string op;


  file_name = argv[1];

  init_size = std::atol(argv[2]);

  table_list = new FilterList(init_size);

  struct timeval t_i1, t_i2;
  struct timeval t_r1, t_r2;

  uint64_t key;
  uint64_t val;

  std::ifstream file_in;
  file_in.open(file_name);


  while(file_in >> op) {
    file_in >> key;

    if(op == "INSERT") {
      gettimeofday(&t_i1, NULL);
      num_inserts += table_list->insert(key, key);
      gettimeofday(&t_i2, NULL);
      elapsed_time_i += (t_i2.tv_sec - t_i1.tv_sec) + (t_i2.tv_usec - t_i1.tv_usec) / 1e6;
    } else {
      gettimeofday(&t_r1, NULL);
      table_list->find(key, val);
      gettimeofday(&t_r2, NULL);
      elapsed_time_r += (t_r2.tv_sec - t_r1.tv_sec) + (t_r2.tv_usec - t_r1.tv_usec) / 1e6;
    }
  }


  std::cout << std::string(file_name) << ", "
            << num_inserts << ", "
            << elapsed_time_r << ", "
            << elapsed_time_i << ", "
            << elapsed_time_u << ", "
            << elapsed_time_u + elapsed_time_i + elapsed_time_r << std::endl;
  return 0;
}
