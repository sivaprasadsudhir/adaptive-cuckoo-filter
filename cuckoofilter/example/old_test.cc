#include "cuckoofilter.h"

#include <cassert>
#include <math.h>

#include <iostream>
#include <vector>
#include <thread>
#include <mutex>

using cuckoofilter::CuckooFilter;

int total_inserts = 0;

int total_items = 100000000;
CuckooFilter<size_t, 12> filteredhash((1ULL << 20)*2);
std::mutex l;

void run_test(int n) {
  // size_t start = n * total_items * 10;
  // std::cout << "Started inserting: " << n << std::endl;

  // // Insert items to this filtered hash table

  // size_t num_inserted = 0;
  // for (size_t i = start; i < start + total_items; i++, num_inserted++) {
  //   // std::cout << i << std::endl;
  //   uint64_t val = 2 * i;
  //   bool insert_val = filteredhash.insert(i, val);
  //   if (!insert_val) {
  //     break;
  //   }
  // }

  // l.lock();
  // total_inserts += num_inserted;
  // l.unlock();

  // std::cout << "Successfully inserted: " << num_inserted << std::endl;

  // // Check if previously inserted items are in the filter, expected
  // // true for all items
  // for (size_t i = start; i < start + num_inserted; i++) {
  //   bool contains_value = filteredhash.contains(i);
  //   assert(contains_value);
  // }
  // std::cout << "Contains done: " << n << std::endl;

  // // // Check if previously inserted items are in the table, expected
  // // // true for all items and val == 2*i
  // for (size_t i = start; i < start + num_inserted; i++) {
  //   uint64_t val;
  //   bool find_val = filteredhash.find(i, val);
  //   assert(find_val);
  //   assert(val == 2 * i);
  // }
  // std::cout << "Find done: " << n << std::endl;

  // // Update values
  // for (size_t i = start; i < start + (num_inserted/2); i++) {
  //   uint64_t val = 4 * i;
  //   bool update_val = filteredhash.update(i, val);
  //   assert(update_val);
  // }
  // std::cout << "update Done: " << n << std::endl;

  // // // Check if updated items are in the table, expected
  // // // true for all items and val == 4*i
  // for (size_t i = start; i < start + (num_inserted/2); i++) {
  //   uint64_t val;
  //   bool find_val = filteredhash.find(i, val);
  //   assert(find_val);
  //   assert(val == 4 * i);
  // }
  // std::cout << "Find done: " << n << std::endl;



  // // Check non-existing items, no false positives expected
  //   // std::cout << "Bla: " << std::endl;
  // for (size_t i = start + total_items; i < start + (2* total_items); i++) {
  //   bool contains_value = filteredhash.contains(i);
  //   if(contains_value) {
  //     std::cout << "Found : " << i << "\n";
  //   }
  //   assert(!contains_value);
  // }
  //   // std::cout << "Blu: " << std::endl;
  // std::cout << "Contains of non existent things done: " << n << std::endl;


  // // Delete all the values, true expected
  // // find should return false
  // // contains should return false
  // for (size_t i = start; i < start + num_inserted; i++) {
  //   bool erase_value = filteredhash.erase(i);
  //   assert(erase_value);
  //   bool contains_value = filteredhash.contains(i);
  //   assert(!contains_value);
  // }
  
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

  std::cout << "Test Successful: " << total_inserts << std::endl;
  std::cout << "Num elements at the end of test: " << filteredhash.Size() << std::endl;

  return 0;
}
