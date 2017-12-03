#include "cuckoofilter.h"

#include <assert.h>
#include <math.h>

#include <iostream>
#include <vector>

using cuckoofilter::CuckooFilter;

int main(int argc, char **argv) {
  int total_items = 1000000;

  CuckooFilter<int, 12> filteredhash(total_items);

  // Insert items to this filtered hash table
  int num_inserted = 0;
  for (int i = 0; i < total_items; i++, num_inserted++) {
    uint64_t val = 2 * i;
    if (!filteredhash.insert(i, val)) {
      break;
    }
  }

  std::cout << "Successfully inserted: " << num_inserted << std::endl;

  // Check if previously inserted items are in the filter, expected
  // true for all items
  for (int i = 0; i < num_inserted; i++) {
    assert(filteredhash.contains(i));
  }
  std::cout << "Contains done: " << std::endl;

  // // Check if previously inserted items are in the table, expected
  // // true for all items and val == -i
  for (int i = 0; i < num_inserted; i++) {
    uint64_t val;
    assert(filteredhash.find(i, val));
    assert(val == 2 * i);
  }
  std::cout << "Find done: " << std::endl;

  // Check non-existing items, no false positives expected
    // std::cout << "Bla: " << std::endl;
  for (int i = total_items; i < 10 * total_items; i++) {
    uint64_t val;
    assert(!filteredhash.find(i, val));
  }
    // std::cout << "Blu: " << std::endl;
  std::cout << "Contains of non existent things done: " << std::endl;


  // Delete all the values, true expected
  // find should return false
  // contains should return false
  for (int i = 0; i < num_inserted; i++) {
    assert(filteredhash.erase(i));
    uint64_t val;
    assert(!filteredhash.find(i, val));
    assert(!filteredhash.contains(i));
  }

  std::cout << "Test Successful" << std::endl;

  return 0;
}
