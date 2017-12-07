#include "cuckoofilter.h"

#include <assert.h>
#include <math.h>

#include <iostream>
#include <vector>
#include <thread>

using cuckoofilter::CuckooFilter;

void run_test(int n) {
  int total_items = 10000000;
  int start = n * total_items * 10;

  CuckooFilter<int, 12> filteredhash(total_items);

  std::cout << "Started inserting: " << n << std::endl;

  // Insert items to this filtered hash table
  int num_inserted = 0;
  for (int i = start; i < start + total_items; i++, num_inserted++) {
    uint64_t val = 2 * i;
    if (!filteredhash.insert(i, val)) {
      break;
    }
  }

  std::cout << "Successfully inserted: " << num_inserted << std::endl;

  // Check if previously inserted items are in the filter, expected
  // true for all items
  for (int i = start; i < start + num_inserted; i++) {
    assert(filteredhash.contains(i));
  }
  std::cout << "Contains done: " << n << std::endl;

  // // Check if previously inserted items are in the table, expected
  // // true for all items and val == -i
  for (int i = start; i < start + num_inserted; i++) {
    uint64_t val;
    assert(filteredhash.find(i, val));
    assert(val == 2 * i);
  }
  std::cout << "Find done: " << n << std::endl;

  // Check non-existing items, no false positives expected
    // std::cout << "Bla: " << std::endl;
  for (int i = start + total_items; i < start + 10 * total_items; i++) {
    uint64_t val;
    assert(!filteredhash.find(i, val));
  }
    // std::cout << "Blu: " << std::endl;
  std::cout << "Contains of non existent things done: " << n << std::endl;


  // Delete all the values, true expected
  // find should return false
  // contains should return false
  for (int i = start; i < start + num_inserted; i++) {
    // std::cout << "Bla1: " << i << std::endl;
    assert(filteredhash.erase(i));
    // std::cout << "Bla2: " << i << std::endl;
    uint64_t val;
    assert(!filteredhash.find(i, val));
    // std::cout << "Bla3: " << i << std::endl;
    assert(!filteredhash.contains(i));
    // std::cout << "Bla4: " << i << std::endl;
  }
  
}

int main(int argc, char **argv) {

  std::thread t1(run_test, 0);
  std::thread t2(run_test, 1);
  std::thread t3(run_test, 2);
  std::thread t4(run_test, 3);
  std::thread t5(run_test, 4);
  t1.join();
  t2.join();
  t3.join();
  t4.join();
  t5.join();

  std::cout << "Test Successful" << std::endl;

  return 0;
}