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

using cuckoofilter::CuckooFilter;

char *file_name;
std::mutex time_lock;
CuckooFilter<uint64_t, 12> *table;
uint64_t init_size;
int num_threads;
double elapsed_time_r = 0.0;
double elapsed_time_i = 0.0;
double elapsed_time_u = 0.0;
uint64_t num_inserts = 0;

void test_read() {

  std::ifstream file_in;
  file_in.open(file_name);

  std::string op;
  uint64_t key;
  uint64_t val;

  double elapsed_time = 0.0;

  struct timeval t_r1, t_r2;

  while(file_in >> op) {
    file_in >> key;
    if(op == "READ") {
      gettimeofday(&t_r1, NULL);
      table->find(key, val);
      gettimeofday(&t_r2, NULL);
      elapsed_time += (t_r2.tv_sec - t_r1.tv_sec) + (t_r2.tv_usec - t_r1.tv_usec) / 1e6;
    }
  }

  time_lock.lock();
  elapsed_time_r += elapsed_time;
  time_lock.unlock();
}

void test_mutate() {
  uint64_t key;
  uint64_t val;

  std::ifstream file_in;
  file_in.open(file_name);


  double time_elapsed_u = 0.0;
  double time_elapsed_i = 0.0;

  struct timeval t_u1, t_u2;
  struct timeval t_i1, t_i2;

  std::string op;

  while(file_in >> op) {
    file_in >> key;
    if (op == "INSERT") {
      gettimeofday(&t_i1, NULL);
      num_inserts += table->insert(key, key);
      gettimeofday(&t_i2, NULL);
      time_elapsed_i += (t_i2.tv_sec - t_i1.tv_sec) + (t_i2.tv_usec - t_i1.tv_usec) / 1e6;
    } else if (op == "UPDATE") {
      gettimeofday(&t_u1, NULL);
      num_inserts += table->update(key, key);
      gettimeofday(&t_u2, NULL);
      time_elapsed_u += (t_u2.tv_sec - t_u1.tv_sec) + (t_u2.tv_usec - t_u1.tv_usec) / 1e6;
    }
  }

  time_lock.lock();
  elapsed_time_i += time_elapsed_i;
  elapsed_time_u += time_elapsed_u;
  time_lock.unlock();
}

int main(int argc, char **argv) {

  if(argc < 4) {
    std::cout << "Arguments not supplied properly" << std::endl;
    exit(1);
  }

  file_name = argv[1];

  init_size = std::atol(argv[2]);

  num_threads = std::atoi(argv[3]);

  table = new CuckooFilter<uint64_t, 12>(init_size);

  std::vector<std::thread> threads;

  for(int i = 0; i < num_threads; i++) {
    if(i == 0) {
      threads.push_back(std::thread(test_mutate));
    } else {
      threads.push_back(std::thread(test_read));
    }
  }

  for(int i = 0; i < num_threads; i++) {
    threads[i].join();
  }

  std::cout << std::string(file_name) << ", "
            << num_inserts << ", "
            << elapsed_time_r << ", "
            << elapsed_time_i << ", "
            << elapsed_time_u << ", "
            << elapsed_time_u + elapsed_time_i + elapsed_time_r << std::endl;
  return 0;
}
