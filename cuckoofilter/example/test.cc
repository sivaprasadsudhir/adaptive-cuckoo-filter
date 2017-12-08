#include "cuckoofilter.h"
#include <libcuckoo/cuckoohash_map.hh>

#include <assert.h>
#include <math.h>
#include <sys/time.h>

#include <cstdlib>
#include <iostream>
#include <vector>
#include <thread>

using cuckoofilter::CuckooFilter;

int main(int argc, char **argv) {

  srand (time(NULL));

  size_t s_size, a_size, ratio_size, query_size;
  size_t offset;

  ratio_size = 1;
  int ratio[] = {10};

  // ratio_size = 13;
  // int ratio[] = {1, 2, 3, 4, 5, 10, 20, 30, 40, 50, 60, 70, 100};

  query_size = 1;
  uint64_t query[] = {100000000};
  
  s_size = 10000;
  // std::vector<int> s(s_size);
  // std::vector<int> a(s_size * 100);

  // CuckooFilter<int, 12> table(s_size);

  // for(int i = 0; i < s_size; i++) {
  //   s[i] = i;
  //   table.insert(s[i], 2 * s[i]);
  // }

  offset = s_size;
  // a_size = s_size * 100;

  // for(int i = 0; i < a_size; i++)
  //   a[i] = offset + i;

  // std::cout << "Ratio\t\tn="<< query[0] << "\t\tn="<< query[1] << "\t\tn="<< query[2] << "" << std::endl;
    
  for(int i = 0; i < ratio_size; i++) {
    a_size = ratio[i] * s_size;

    std::cout << ratio[i] << "\t\t";

    struct timeval t1, t2, t3;

    for(int j = 0; j < query_size; j++) {

      gettimeofday(&t1, NULL);

      CuckooFilter<int, 12> table(s_size);
      // cuckoohash_map<int, uint64_t> table;

      for(int i = 0; i < s_size; i++) {
        table.insert(i, 2 * i);
      }

      gettimeofday(&t2, NULL);

      int false_positives = 0;

      for(uint64_t k = 0; k < query[j]; k++) {
        int key = offset + rand() % a_size;
        uint64_t val;

        // int found_in_filter = table.findinfilter(key);
        int found = table.find(key, val);

        assert(!found);

        // int key = rand() % s_size;
        // uint64_t val;

        // // int found_in_filter = table.findinfilter(key);
        // int found = table.find(key, val);

        // assert(found);
        // assert(val == 2 * key);

        // false_positives += found_in_filter && !found;
        
      }

      gettimeofday(&t3, NULL);

      std::cout << 100 * (false_positives * 1.0) / query[j] << "\t\t";
    }

    std::cout << std::endl;
    std::cout << "Time taken for insert: " << (t2.tv_sec - t1.tv_sec) + (t2.tv_usec - t1.tv_usec) / 1e6 << std::endl;
    std::cout << "Time taken for lookup: " << (t3.tv_sec - t2.tv_sec) + (t3.tv_usec - t2.tv_usec) / 1e6 << std::endl;
  }

  std::cout << "Test Successful" << std::endl;

  return 0;
}
