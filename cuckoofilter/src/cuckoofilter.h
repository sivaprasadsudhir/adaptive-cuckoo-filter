#ifndef CUCKOO_FILTER_CUCKOO_FILTER_H_
#define CUCKOO_FILTER_CUCKOO_FILTER_H_

#include <assert.h>
#include <algorithm>
#include <libcuckoo/cuckoohash_map.hh>
#include <mutex>
#include <atomic>

#include "debug.h"
#include "hashutil.h"
#include "packedtable.h"
#include "printutil.h"
#include "singletable.h"

namespace cuckoofilter {
// status returned by a cuckoo filter operation
enum Status {
  Ok = 0,
  NotFound = 1,
  NotEnoughSpace = 2,
  NotSupported = 3,
};

// maximum number of cuckoo kicks before claiming failure
const size_t kMaxCuckooCount = 500;

// A cuckoo filter class exposes a Bloomier filter interface,
// providing methods of Add, Delete, Contain. It takes three
// template parameters:
//   ItemType:  the type of item you want to insert
//   bits_per_item: how many bits each item is hashed into
//   TableType: the storage of table, SingleTable by default, and
// PackedTable to enable semi-sorting
template <typename ItemType, size_t bits_per_item,
          template <size_t> class TableType = SingleTable,
          typename HashFamily = TwoIndependentMultiplyShift>
class CuckooFilter {
  // Storage of items
  TableType<bits_per_item> *table_;
  cuckoohash_map<ItemType, uint64_t> hashmap;

  // Number of items stored
  size_t num_items_;

  typedef struct {
    size_t index;
    uint64_t tag_hash;
    std::atomic_bool used;
    ItemType key;
    uint64_t val;
  } VictimCache;

  VictimCache victim_;

  HashFamily hasher_;
  std::mutex write_lock;

  // inline size_t IndexHash(uint32_t hv) const {
    // table_->num_buckets is always a power of two, so modulo can be replaced
    // with
    // bitwise-and:
    // return hv & (table_->NumBuckets() - 1);
  // }

  inline void TagHash(uint64_t hv, uint32_t tag[4]) const
  {
    for (int i = 0; i < 4; i++) {
      tag[i] = hv & ((1ULL << bits_per_item) - 1);
      tag[i] += (tag[i] == 0);
      hv = hv >> bits_per_item;
    }
  }

  inline void GenerateIndexTagHash(const ItemType& key, uint32_t* index1,
            uint32_t* index2, uint32_t tag[4], uint64_t &tag_hash) const
  {
    *index2 = *index1 = 0;
    HashUtil::BobHash((&key), sizeof(ItemType), index1, index2);
    *index1 = *index1 & (table_->NumBuckets() - 1);
    *index2 = *index2 & (table_->NumBuckets() - 1);
    tag_hash = hasher_(key);
    TagHash(tag_hash, tag);
    // if(key == 0 || key == 1) {
      // std::cout << tag
      // std::cout << key << " ";
      // for(int i = 0; i < 4; i++)
        // std::cout << tag[i] << " ";
      // std::cout << *index1 << " " << *index2;
      // std::cout << std::endl;
    // }
  }

  // load factor is the fraction of occupancy
  double LoadFactor() const { return 1.0 * Size() / table_->SizeInTags(); }

  double BitsPerItem() const { return 8.0 * table_->SizeInBytes() / Size(); }

 public:
  explicit CuckooFilter(const size_t max_num_keys) : hashmap(), num_items_(0), victim_(), hasher_()
  {
    size_t assoc = 4;
    size_t max_num_keys_1 = (1U << 16) * 2;
    size_t num_buckets = upperpower2(std::max<uint64_t>(1, max_num_keys_1 / assoc));
    double frac = (double)max_num_keys_1 / num_buckets / assoc;
    if (frac > 0.96) {
      num_buckets <<= 1;
    }
    victim_.used = false;
    table_ = new TableType<bits_per_item>(num_buckets);
    // std::cout << "Bits per Item " << bits_per_item << std::endl;
    // hashmap = new cuckoohash_map<ItemType, uint64_t>(max_num_keys);
  }

  ~CuckooFilter()
  {
    delete table_;
  }

  /* methods for providing stats  */
  // summary infomation
  std::string Info() const;

  // number of current inserted items;
  size_t Size() const { return num_items_; }

  // size of the filter in bytes.
  size_t SizeInBytes() const { return table_->SizeInBytes(); }

  bool find(const ItemType &key, uint64_t& val);
  bool findinfilter(const ItemType &key);
  bool contains(const ItemType &key);
  bool insert(const ItemType &key, const uint64_t &val);
  bool insert_impl(const ItemType &key, const uint64_t &val, size_t i, uint32_t tag[4], uint64_t taghash, bool has_lock);
  bool erase(const ItemType &key);
  void remove_false_positives(size_t index, size_t slot);

};

template <typename ItemType, size_t bits_per_item,
          template <size_t> class TableType, typename HashFamily>
std::string CuckooFilter<ItemType, bits_per_item, TableType, HashFamily>::Info() const
{
  std::stringstream ss;
  ss << "CuckooFilter Status:\n"
     << "\t\t" << table_->Info() << "\n"
     << "\t\tKeys stored: " << Size() << "\n"
     << "\t\tLoad factor: " << LoadFactor() << "\n"
     << "\t\tHashtable size: " << (table_->SizeInBytes() >> 10) << " KB\n";
  if (Size() > 0) {
    ss << "\t\tbit/key:   " << BitsPerItem() << "\n";
  } else {
    ss << "\t\tbit/key:   N/A\n";
  }
  return ss.str();
}
  
template <typename ItemType, size_t bits_per_item,
          template <size_t> class TableType, typename HashFamily>
bool CuckooFilter<ItemType, bits_per_item, TableType, HashFamily>::find(
                                const ItemType &key, uint64_t& val)
{

  bool found = false;
  uint32_t i1, i2;
  uint32_t tag[4];
  uint64_t tag_hash;

  GenerateIndexTagHash(key, &i1, &i2, tag, tag_hash);

  // if(atomic_load(&victim_.used)) {
    // write_lock.lock();
    found = victim_.used && (key == victim_.key) &&
          (i1 == victim_.index || i2 == victim_.index);
    if (found) {
      val = victim_.val;
      // write_lock.unlock();
      return true;
    }
    // write_lock.unlock();
  // }

  // TODO[Siva]: Decide what needs to be stores in false_positives
  // std::vector< std::pair<size_t, size_t> > false_positives_1, false_positives_2;

  // check in i1
  // uint16_t c1, c2;
  
  // do {
    // c1 = table_->read_even_counter(i1);
    // false_positives_1.clear();

    int bla = 0;

    for (int slot = 0; slot < 4; slot++) {
      // std::cout << "Checking find for key: " << key << " in bucket " << i1 << "," << slot << " and got " << table_->ReadTag(i1, slot) << ", expected " << tag[slot] << std::endl;
      if(tag[slot] == table_->ReadTag(i1, slot)) {
        std::pair<ItemType, uint64_t> key_value;
        hashmap.read_from_bucket_at_slot(i1, slot, key_value);
        bla++;
        // std::cout << "Finger print matched and hashmap gave " << key_value.first << " " << key_value.second << std::endl;
        if(key == key_value.first) {
          val = key_value.second;
          found = true;
          // goto find_false_positive_removal;
        }
        else {
          // std::cout << "False Positives" << std::endl;
          // std::cout << "But keys didn't match " << key_value.first << " " << key << std::endl;
          // false_positives_1.push_back(std::make_pair(i1, slot));
        }
      }
    }
  // } while(table_->counter_changed(i1, c1));

  if(bla > 1)
    std::cout << "ALERT" << std::endl;

  bla = 0;
  // check in i2
  // do {
    // c2 = table_->read_even_counter(i2);
    // false_positives_2.clear();
    for (int slot = 0; slot < 4; slot++) {
      // std::cout << "Checking find for key: " << key << " in bucket " << i2 << "," << slot << " and got " << table_->ReadTag(i2, slot) << ", expected " << tag[slot] << std::endl;

      if(tag[slot] == table_->ReadTag(i2, slot)) {
        std::pair<ItemType, uint64_t> key_value;
        hashmap.read_from_bucket_at_slot(i2, slot, key_value);
        bla++;
        // std::cout << "Finger print matched and hashmap gave " << key_value.first << " " << key_value.second << std::endl;
        if(key == key_value.first) {
          val = key_value.second;
          found = true;
          // goto find_false_positive_removal;
        }
        else {
          // std::cout << "False Positives" << std::endl;
          // std::cout << "But keys didn't match " << key_value.first << " " << key << std::endl;
          // false_positives_2.push_back(std::make_pair(i2, slot));
        }
      }
    }

     if(bla > 1)
    std::cout << "ALERT" << std::endl;
  // } while(table_->counter_changed(i2, c2));

  // if(false_positives_1.size() != 0) {
  //   // if(false_positives_1.size() > 1) {
  //   //   std::cout << "ERROR" << std::endl;
  //     // exit(1);
  //   // }
  //   // table_->increment_odd(i1);
  //   for(unsigned int i = 0; i < false_positives_1.size(); i++) {
  //     // std::cout << "Called remove_false_positives " << std::endl;
  //     remove_false_positives(false_positives_1[i].first, false_positives_1[i].second);
  //   }
  //   // table_->increment_even(i1);
  // }

  // if(false_positives_2.size() != 0) {
  //   // if(false_positives_2.size() > 1) {
  //   //   std::cout << "ERROR" << std::endl;
  //     // exit(1);
  //   // }
  //   // table_->increment_odd(i2);
  //   for(unsigned int i = 0; i < false_positives_2.size(); i++) {
  //     // std::cout << "Called remove_false_positives " << std::endl;
  //     remove_false_positives(false_positives_2[i].first, false_positives_2[i].second);
  //   }
  //   // table_->increment_even(i2);
  // }

  return found;

  // find_false_positive_removal:
  // // call false positive removal for each pair in false_positives
  // for(unsigned int i = 0; i < false_positives.size(); i++) {
  //   std::cout << "Called remove_false_positives " << std::endl;
  //   remove_false_positives(false_positives[i].first, false_positives[i].second);
  // }
  // return true;

}

template <typename ItemType, size_t bits_per_item,
          template <size_t> class TableType, typename HashFamily>
bool CuckooFilter<ItemType, bits_per_item, TableType, HashFamily>::findinfilter(
                                const ItemType &key)
{

  bool found = false;
  uint32_t i1, i2;
  uint32_t tag[4];
  uint64_t tag_hash;

  GenerateIndexTagHash(key, &i1, &i2, tag, tag_hash);

  // if(atomic_load(&victim_.used)) {
    // write_lock.lock();
    found = victim_.used && (key == victim_.key) &&
            (i1 == victim_.index || i2 == victim_.index);
    if (found) {
      // write_lock.unlock();
      return true;
    }
    // write_lock.unlock();
  // }


  // uint16_t c1, c2;
  // int counter = 0;
  
  // check in i1
  // do {
    // c1 = table_->read_even_counter(i1);
    for (int slot = 0; slot < 4; slot++) {
      if(tag[slot] == table_->ReadTag(i1, slot)) {
        return true;
      }
    }
    // counter++;
  // } while(table_->counter_changed(i1, c1));

  // std::cout << "Ran " << counter << " ";
  // check in i2
  // do {
    // c2 = table_->read_even_counter(i2);
    for (int slot = 0; slot < 4; slot++) {
      if(tag[slot] == table_->ReadTag(i2, slot)) {
        return true;
      }
    }
    // counter++;
  // } while(table_->counter_changed(i2, c2));

  // std::cout << "and " << counter << " times" << std::endl;

  return false;
}


template <typename ItemType, size_t bits_per_item,
          template <size_t> class TableType, typename HashFamily>
bool CuckooFilter<ItemType, bits_per_item, TableType, HashFamily>::contains(
                                                  const ItemType &key)
{
  uint64_t val;
  return find(key, val);
}

template <typename ItemType, size_t bits_per_item,
          template <size_t> class TableType, typename HashFamily>
bool CuckooFilter<ItemType, bits_per_item, TableType, HashFamily>::insert(
                                          const ItemType &key, const uint64_t &val)

{
  uint32_t i1, i2;
  uint32_t tag[4];
  uint64_t tag_hash;

  if (atomic_load(&victim_.used)) {
    return false;
  }

  GenerateIndexTagHash(key, &i1, &i2, tag, tag_hash);
  return insert_impl(key, val, i1, tag, tag_hash, false);
}

template <typename ItemType, size_t bits_per_item,
          template <size_t> class TableType, typename HashFamily>
bool CuckooFilter<ItemType, bits_per_item, TableType, HashFamily>::insert_impl(
  const ItemType &key, const uint64_t &val, size_t i, uint32_t curtag[4], uint64_t curtaghash, bool has_lock)
{
  uint32_t i1, i2;
  size_t curindex = i;
  ItemType curkey = key;
  uint64_t curval = val;
  size_t slot;

  for (uint32_t count = 0; count < kMaxCuckooCount; count++) {
    bool kickout = count > 0;
    slot = -1;

    if(count == 1 && !has_lock) {
      write_lock.lock();
      if(victim_.used) {
        write_lock.unlock();
        return false;
      }
    }

    table_->increment_odd(curindex);

    if (table_->InsertTagToBucket(curindex, curtag, kickout, slot)) {
      std::pair<ItemType, uint64_t> key_value;
      // std::cout<< "ReadTag after write " << curindex << " " << slot << " " << table_->ReadTag(curindex, slot) << "\n";
      hashmap.add_to_bucket_at_slot(curindex, slot, curkey, curval);

      hashmap.read_from_bucket_at_slot(curindex, slot, key_value);

      //std::cout << " " << key_value.first << " " << curkey << " " << key_value.second << " " << curval << " " << std::endl;
      assert(key_value.first == curkey && key_value.second == curval);
  // std::cout << "Here4" << std::endl;

      num_items_++;

      table_->increment_even(curindex);
      if(kickout && !has_lock)
        write_lock.unlock();

      return true;
    }
    
    if (kickout) {
      std::pair<ItemType, uint64_t> old_key_value;
      hashmap.read_from_bucket_at_slot(curindex, slot, old_key_value);
      hashmap.add_to_bucket_at_slot(curindex, slot, curkey, curval);
      curkey = old_key_value.first;
      curval = old_key_value.second;
      // std::cout << "Kicked out" << curkey << " " << curval << std::endl;
    }

    table_->increment_even(curindex);

    GenerateIndexTagHash(curkey, &i1, &i2, curtag, curtaghash);
    curindex = (curindex == i1) ? i2 : i1;
  }

  victim_.index = curindex;
  victim_.tag_hash = curtaghash;
  victim_.key = curkey;
  victim_.val = curval;
  victim_.used = true;
  if(!has_lock)
    write_lock.unlock();
  return true;
}

template <typename ItemType, size_t bits_per_item,
          template <size_t> class TableType, typename HashFamily>
bool CuckooFilter<ItemType, bits_per_item, TableType, HashFamily>::erase(
    const ItemType &key)
{

  bool found = false;
  uint32_t i1, i2;
  uint32_t tag[4];
  uint64_t tag_hash;

  GenerateIndexTagHash(key, &i1, &i2, tag, tag_hash);

  if(atomic_load(&victim_.used)) {
    write_lock.lock();
    found = victim_.used && (key == victim_.key) &&
            (i1 == victim_.index || i2 == victim_.index);

    if (found) {
      victim_.used = false;
      write_lock.unlock();
      return true;
    }
    write_lock.unlock();
  }

  
  // TODO[Siva]: Decide what needs to be stores in false_positives
  std::vector< std::pair<size_t, size_t> > false_positives;

  // check in i1

  table_->increment_odd(i1);
  for (int slot = 0; slot < 4; slot++) {
    if(tag[slot] == table_->ReadTag(i1, slot)) {
      std::pair<ItemType, uint64_t> key_value;
      hashmap.read_from_bucket_at_slot(i1, slot, key_value);
      if(key == key_value.first) {
        table_->WriteTag(i1, slot, 0);
        hashmap.del_from_bucket_at_slot(i1, slot);
        found = true;
        // goto delete_false_positive_removal;
      }
      else {
        false_positives.push_back(std::make_pair(i1, slot));
      }
    }
  }

  for(unsigned int i = 0; i < false_positives.size(); i++) {
    remove_false_positives(false_positives[i].first, false_positives[i].second);
  }
  false_positives.clear();

  table_->increment_even(i1);
  
  // check in i2
  table_->increment_odd(i2);
  for (int slot = 0; slot < 4; slot++) {
    if(tag[slot] == table_->ReadTag(i2, slot)) {
      std::pair<ItemType, uint64_t> key_value;
      hashmap.read_from_bucket_at_slot(i2, slot, key_value);
      if(key == key_value.first) {
        table_->WriteTag(i2, slot, 0);
        hashmap.del_from_bucket_at_slot(i2, slot);
        found = true;
        // goto delete_false_positive_removal;
      }
      else {
        false_positives.push_back(std::make_pair(i2, slot));
      }
    }
  }

  for(unsigned int i = 0; i < false_positives.size(); i++) {
    remove_false_positives(false_positives[i].first, false_positives[i].second);
  }

  table_->increment_even(i2);

  if(!found)
    return false;

  // Try removing victim

  if (victim_.used) {
    write_lock.lock();
    victim_.used = false;
    uint32_t tag[4];
    TagHash(victim_.tag_hash, tag);
    insert_impl(victim_.key, victim_.val, victim_.index, tag, victim_.tag_hash, true);
    write_lock.unlock();
  }

  return true;
}

template <typename ItemType, size_t bits_per_item,
          template <size_t> class TableType, typename HashFamily>
void CuckooFilter<ItemType, bits_per_item, TableType, HashFamily>::remove_false_positives(
                                                                size_t index, size_t slot)
{
  // return;
  // std::cout << "Remove False Positives" << std::endl;

  int new_slot = rand() % 3;
  if(new_slot == slot)
    new_slot = 3;

  // std::cout << "AAA " << index << " " << slot << " " << new_slot << std::endl;

  bool empty_new_slot = (table_->ReadTag(index, new_slot) == 0);

  // if(empty_new_slot) 
    // std::cout << "New slot empty" << std::endl;
  // else 
    // std::cout << "New slot not empty" << std::endl;

  std::pair<ItemType, uint64_t> key_value_slot;
  hashmap.read_from_bucket_at_slot(index, slot, key_value_slot);
  
  std::pair<ItemType, uint64_t> key_value_new_slot;
  if(!empty_new_slot)
    hashmap.read_from_bucket_at_slot(index, new_slot, key_value_new_slot);

  uint32_t temp_index;
  uint64_t tag_hash;
  uint32_t tag_slot[4];
  uint32_t tag_new_slot[4];

  GenerateIndexTagHash(key_value_slot.first, &temp_index, &temp_index, tag_slot, tag_hash);
  if(!empty_new_slot)
    GenerateIndexTagHash(key_value_new_slot.first, &temp_index, &temp_index, tag_new_slot, tag_hash);

  if(!empty_new_slot)
    table_->WriteTag(index, slot, tag_new_slot[slot]);
  else
    table_->WriteTag(index, slot, 0);
  table_->WriteTag(index, new_slot, tag_slot[new_slot]);

  
  if(!empty_new_slot)
    hashmap.add_to_bucket_at_slot(index, slot, key_value_new_slot.first, key_value_new_slot.second);
  else
    hashmap.del_from_bucket_at_slot(index, slot);
  hashmap.add_to_bucket_at_slot(index, new_slot, key_value_slot.first, key_value_slot.second);

  // std::cout << "Done with Remove False Positives" << std::endl;

}

}  // namespace cuckoofilter
#endif  // CUCKOO_FILTER_CUCKOO_FILTER_H_
