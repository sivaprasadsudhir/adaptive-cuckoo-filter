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


const size_t one = 1;
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
  std::atomic_size_t num_items_;

  // Number of buckets
  size_t num_buckets_;

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

  inline void TagHash(uint64_t hv, uint16_t tag[4]) const
  {
    hv >>= 16;
    for (int i = 0; i < 4; i++) {
      tag[3-i] = hv & ((1ULL << bits_per_item) - 1);
      tag[3-i] += (tag[3-i] == 0);
      hv = hv >> bits_per_item;
    }
  }

  inline void GenerateIndexTagHash(const ItemType& key, uint32_t* index1,
            uint32_t* index2, uint16_t* tag, uint64_t &tag_hash) const
  {

    uint64_t hash1 = hasher_(key);
    *index1 = (hash1 >> 32) & (table_->NumBuckets() - 1);

    uint64_t hash2 = hasher_(*index1 ^ ((uint16_t)hash1 * 0x5bd1e995));
    *index2 = ((uint32_t)hash2) & (table_->NumBuckets() - 1);

    tag_hash = (hash2 & 0xffffffff00000000) | (hash1 & 0xffffffff);

    uint64_t mask = ~(0xffff);
    tag_hash &= mask;

    uint64_t part_mask = 0xfff << 16;
    // 4
    tag_hash += ((uint64_t)!(tag_hash & (part_mask))) << 16;
    tag[0] = (tag_hash >> 16) & 0xfff;
    // 3
    part_mask <<= 12;
    tag_hash += ((uint64_t)!(tag_hash & (part_mask))) << (16 + 12);
    tag[1] = (tag_hash >> (16 + 12)) & 0xfff;
    // 2
    part_mask <<= 12;
    tag_hash += ((uint64_t)!(tag_hash & (part_mask))) << (16 + 12 + 12);
    tag[2] = (tag_hash >> (16 + 12 + 12)) & 0xfff;
    // 1
    part_mask <<= 12;
    tag_hash += ((uint64_t)!(tag_hash & (part_mask))) << (16 + 12 + 12 + 12);
    tag[3] = (tag_hash >> (16 + 12 + 12 + 12)) & 0xfff;
  }

  // load factor is the fraction of occupancy
  double LoadFactor() const { return 1.0 * Size() / table_->SizeInTags(); }

  double BitsPerItem() const { return 8.0 * table_->SizeInBytes() / Size(); }

 public:
  explicit CuckooFilter(const size_t max_num_keys) : hashmap(max_num_keys * 2), num_items_(0), victim_(), hasher_()
  {
    size_t assoc = 4;
    size_t num_buckets = upperpower2(std::max<uint64_t>(1, max_num_keys / assoc));
    double frac = (double)max_num_keys / num_buckets / assoc;
    if (frac > 0.96) {
      num_buckets <<= 1;
    }
    atomic_store(&victim_.used, false);
    table_ = new TableType<bits_per_item>(num_buckets);
  }

  ~CuckooFilter()
  {
    delete table_;
  }

  /* methods for providing stats  */
  // summary infomation
  std::string Info() const;

  // number of current inserted items;
  size_t Size() const { return std::atomic_load_explicit(&num_items_, std::memory_order_seq_cst); }

  // size of the filter in bytes.
  size_t SizeInBytes() const { return table_->SizeInBytes(); }

  bool find(const ItemType &key, uint64_t& val);
  bool findinfilter(const ItemType &key);
  bool contains(const ItemType &key);
  bool insert(const ItemType &key, const uint64_t &val);
  bool insert_impl(const ItemType &key, const uint64_t &val, uint32_t i1, uint32_t i2, uint16_t tag[4], uint64_t taghash);
  bool erase(const ItemType &key);
  void remove_false_positives(size_t index, size_t slot);
  void iterate_all(std::vector<std::pair <ItemType, uint64_t> >& elements); 
  bool update(const ItemType &key, const uint64_t &val);

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
     << "\t\tFilter size: " << (table_->SizeInBytes() >> 10) << " KB\n";
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
  uint16_t tag[4];
  uint64_t tag_hash;

  GenerateIndexTagHash(key, &i1, &i2, tag, tag_hash);
  uint32_t false_positives_index[8];
  uint8_t false_positives_slot[8];
  uint8_t curindex = 0;

  uint16_t c1, c2;
  
  do {
    c1 = table_->read_even_counter(i1);
    c2 = table_->read_even_counter(i2);

    curindex = 0;

    uint64_t bucket = table_->ReadBucket(i1);

    if (bucket == 0) {
      break;
    }

    uint64_t match = ~(bucket ^ (tag_hash >> 16));
    uint16_t mask = 0xfff;

    for(int slot = 0; slot < 4; slot++) {

      if(((match & mask) + 1) & (mask + 1)) {

        std::pair<ItemType, uint64_t> key_value;
        hashmap.read_from_bucket_at_slot(i1, slot, key_value);

        if(key == key_value.first) {
          val = key_value.second;
          found = true;
          goto false_positives_removal;
        } else {
          false_positives_index[curindex] = i1;
          false_positives_slot[curindex] = slot;
          curindex++;
        }
      }
      match >>= 12;
    }

    bucket = table_->ReadBucket(i2);
    match = ~(bucket ^ (tag_hash >> 16));
    mask = 0xfff;
    
    for(int slot = 0; slot < 4; slot++) {

      if(((match & mask) + 1) & (mask + 1)) {
        std::pair<ItemType, uint64_t> key_value;
        hashmap.read_from_bucket_at_slot(i2, slot, key_value);

        if(key == key_value.first) {
          val = key_value.second;
          found = true;
          goto false_positives_removal;
        } else {
          false_positives_index[curindex] = i2;
          false_positives_slot[curindex] = slot;
          curindex++;
        }
      }
      match >>= 12;
    }
  } while(table_->counter_changed(i2, c2) && table_->counter_changed(i2, c2));

  if(atomic_load(&victim_.used) && (key == victim_.key)) {
    val = victim_.val;
    return true;
  }

false_positives_removal:

  if(curindex != 0) {
    table_->increment_odd(i1);
    for(unsigned int i = 0; i < curindex; i++) {
      remove_false_positives(false_positives_index[i], false_positives_slot[i]);
    }
    table_->increment_even(i1);
  }

  return found;
}

template <typename ItemType, size_t bits_per_item,
          template <size_t> class TableType, typename HashFamily>
bool CuckooFilter<ItemType, bits_per_item, TableType, HashFamily>::findinfilter(
                                const ItemType &key)
{
  uint32_t i1, i2;
  uint16_t tag[4];
  uint64_t tag_hash;

  GenerateIndexTagHash(key, &i1, &i2, tag, tag_hash);
  uint16_t c1, c2;
  
  do {
    c1 = table_->read_even_counter(i1);
    c2 = table_->read_even_counter(i2);

    uint64_t bucket = table_->ReadBucket(i1);
    if (bucket == 0) {
      break;
    }
    uint64_t match = ~(bucket ^ (tag_hash >> 16));
    uint16_t mask = 0xfff;

    for(int slot = 0; slot < 4; slot++) {
      if(((match & mask) + 1) & (mask + 1)) {
        return true;
      }
      match >>= 12;
    }

    bucket = table_->ReadBucket(i2);
    match = ~(bucket ^ (tag_hash >> 16));
    mask = 0xfff;
    
    for(int slot = 0; slot < 4; slot++) {
      if(((match & mask) + 1) & (mask + 1)) {
        return true;
      }
      match >>= 12;
    }

  } while(table_->counter_changed(i2, c2) && table_->counter_changed(i2, c2));

  if(atomic_load(&victim_.used) && (key == victim_.key)) {
    return true;
  }

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
  uint16_t tag[4];
  uint64_t tag_hash;

  if (atomic_load(&victim_.used)) {
    return false;
  }

  GenerateIndexTagHash(key, &i1, &i2, tag, tag_hash);
  return insert_impl(key, val, i1, i2, tag, tag_hash);
}

template <typename ItemType, size_t bits_per_item,
          template <size_t> class TableType, typename HashFamily>
bool CuckooFilter<ItemType, bits_per_item, TableType, HashFamily>::insert_impl(
  const ItemType &key, const uint64_t &val, uint32_t i1, uint32_t i2, uint16_t curtag[4], uint64_t curtaghash)
{
  size_t curindex = i1;
  ItemType curkey = key;
  uint64_t curval = val;
  size_t slot;

  table_->increment_odd(curindex);

  if (table_->InsertTagToBucket(curindex, curtag, false, slot)) {
      std::pair<ItemType, uint64_t> key_value;
      hashmap.add_to_bucket_at_slot(curindex, slot, curkey, curval);
      hashmap.read_from_bucket_at_slot(curindex, slot, key_value);
      std::atomic_fetch_add_explicit(&num_items_, one, std::memory_order_seq_cst);
      table_->increment_even(curindex);
      return true;
    }

  table_->increment_even(curindex);

  write_lock.lock();

  if(atomic_load(&victim_.used)) {
    write_lock.unlock();
    return false;
  }

  curindex = i2;
  table_->increment_odd(curindex);


  for (uint32_t count = 1; count < kMaxCuckooCount; count++) {
    slot = -1;

    if (table_->InsertTagToBucket(curindex, curtag, true, slot)) {
      std::pair<ItemType, uint64_t> key_value;
      hashmap.add_to_bucket_at_slot(curindex, slot, curkey, curval);
      hashmap.read_from_bucket_at_slot(curindex, slot, key_value);
      std::atomic_fetch_add_explicit(&num_items_, one, std::memory_order_seq_cst);
      table_->increment_even(curindex);
      write_lock.unlock();
      return true;
    }
    
    std::pair<ItemType, uint64_t> old_key_value;
    hashmap.read_from_bucket_at_slot(curindex, slot, old_key_value);
    hashmap.add_to_bucket_at_slot(curindex, slot, curkey, curval);
    curkey = old_key_value.first;
    curval = old_key_value.second;

    uint32_t oldindex = curindex;

    GenerateIndexTagHash(curkey, &i1, &i2, curtag, curtaghash);
    curindex = (oldindex == i1) ? i2 : i1;

    if(curindex != oldindex) {
      table_->increment_odd(curindex);
      table_->increment_even(oldindex); 
    }
  }

  victim_.index = curindex;
  victim_.tag_hash = curtaghash;
  victim_.key = curkey;
  victim_.val = curval;
  atomic_store(&victim_.used, true);

  table_->increment_even(curindex);

  std::atomic_fetch_add_explicit(&num_items_, one, std::memory_order_seq_cst);

  write_lock.unlock();

  return true;
}

template <typename ItemType, size_t bits_per_item,
          template <size_t> class TableType, typename HashFamily>
bool CuckooFilter<ItemType, bits_per_item, TableType, HashFamily>::erase(
    const ItemType &key)
{

  uint32_t i1, i2;
  uint16_t tag[4];
  uint64_t tag_hash;

  GenerateIndexTagHash(key, &i1, &i2, tag, tag_hash);

  // check in i1

  table_->increment_odd(i1);
  if(i1 != i2) {
    table_->increment_odd(i2);
  }

  for (int slot = 0; slot < 4; slot++) {
    if(tag[slot] == table_->ReadTag(i1, slot)) {
      std::pair<ItemType, uint64_t> key_value;
      hashmap.read_from_bucket_at_slot(i1, slot, key_value);
      if(key == key_value.first) {
        table_->WriteTag(i1, slot, 0);
        hashmap.del_from_bucket_at_slot(i1, slot);
        table_->increment_even(i1);
        if(i1 != i2) {
          table_->increment_even(i2);
        }
        std::atomic_fetch_sub_explicit(&num_items_, one, std::memory_order_seq_cst);
        // std::cout << "Number of elements: " << Size() << "\n";
        return true;
      }
    }
  }

  // check in i2
  for (int slot = 0; slot < 4; slot++) {
    if(tag[slot] == table_->ReadTag(i2, slot)) {
      std::pair<ItemType, uint64_t> key_value;
      hashmap.read_from_bucket_at_slot(i2, slot, key_value);
      if(key == key_value.first) {
        table_->WriteTag(i2, slot, 0);
        hashmap.del_from_bucket_at_slot(i2, slot);
        table_->increment_even(i1);
        if(i1 != i2) {
          table_->increment_even(i2);
        }
        std::atomic_fetch_sub_explicit(&num_items_, one, std::memory_order_seq_cst);
        // std::cout << "Number of elements: " << Size() << "\n";

        return true;
      }
    }
  }

  table_->increment_even(i2);
  if(i1 != i2) {
    table_->increment_even(i1);
  }

  write_lock.lock();
  if(atomic_load(&victim_.used) && (key == victim_.key)) {
    atomic_store(&victim_.used, false);
    write_lock.unlock();
    std::atomic_fetch_sub_explicit(&num_items_, one, std::memory_order_seq_cst);
    // std::cout << "Number of elements: " << Size() << "\n";
    return true;
  }
  write_lock.unlock();

  return false;
}

template <typename ItemType, size_t bits_per_item,
          template <size_t> class TableType, typename HashFamily>
void CuckooFilter<ItemType, bits_per_item, TableType, HashFamily>::remove_false_positives(
                                                                size_t index, size_t slot)
{
  int new_slot = (slot + 1) & 3;
  if(new_slot == slot)
    std::cout << "ALERT" << std::endl;

  bool empty_new_slot = (table_->ReadTag(index, new_slot) == 0);

  std::pair<ItemType, uint64_t> key_value_slot;
  hashmap.read_from_bucket_at_slot(index, slot, key_value_slot);
  
  std::pair<ItemType, uint64_t> key_value_new_slot;
  if(!empty_new_slot)
    hashmap.read_from_bucket_at_slot(index, new_slot, key_value_new_slot);

  uint32_t temp_index;
  uint64_t tag_hash;
  uint16_t tag_slot[4];
  uint16_t tag_new_slot[4];

  GenerateIndexTagHash(key_value_slot.first, &temp_index, &temp_index, tag_slot, tag_hash);
  if(!empty_new_slot)
    GenerateIndexTagHash(key_value_new_slot.first, &temp_index, &temp_index, tag_new_slot, tag_hash);

  table_->WriteTag(index, new_slot, tag_slot[new_slot]);
  if(!empty_new_slot)
    table_->WriteTag(index, slot, tag_new_slot[slot]);
  else
    table_->WriteTag(index, slot, 0);

  
  hashmap.add_to_bucket_at_slot(index, new_slot, key_value_slot.first, key_value_slot.second);
  if(!empty_new_slot)
    hashmap.add_to_bucket_at_slot(index, slot, key_value_new_slot.first, key_value_new_slot.second);
  else
    hashmap.del_from_bucket_at_slot(index, slot);
}

template <typename ItemType, size_t bits_per_item, 
          template <size_t> class TableType, typename HashFamily>
void CuckooFilter<ItemType, bits_per_item, TableType, HashFamily>::iterate_all(
  std::vector<std::pair <ItemType, uint64_t>>& elements) {
  size_t num_items = Size();
  elements.resize(num_items);
  size_t hashmap_items = hashmap.iterate_all(elements, num_items);
  if( num_items == (hashmap_items + 1) ) {
    if(std::atomic_load(&victim_.used)){
      elements[hashmap_items].first = victim_.key;
      elements[hashmap_items].second = victim_.val;
    }
  }
}

template <typename ItemType, size_t bits_per_item,
          template <size_t> class TableType, typename HashFamily>
bool CuckooFilter<ItemType, bits_per_item, TableType, HashFamily>::update(
                                  const ItemType &key, const uint64_t &val) {
  uint32_t i1, i2;
  uint16_t tag[4];
  uint64_t tag_hash;

  GenerateIndexTagHash(key, &i1, &i2, tag, tag_hash);

  table_->increment_odd(i1);
  if(i1 != i2) {
    table_->increment_odd(i2);
  }

  uint64_t bucket = table_->ReadBucket(i1);
  uint64_t match = ~(bucket ^ (tag_hash >> 16));
  uint16_t mask = 0xfff;

  for(int slot = 0; slot < 4; slot++) {

    if(((match & mask) + 1) & (mask + 1)) {
      if(hashmap.update_bucket_at_slot(i1, slot, key, val)) {

        table_->increment_even(i2);
        if(i1 != i2) {
          table_->increment_even(i1);
        }
        return true;

      }
    }
    match >>= 12;
  }

  bucket = table_->ReadBucket(i2);
  match = ~(bucket ^ (tag_hash >> 16));
  mask = 0xfff;
  
  for(int slot = 0; slot < 4; slot++) {

    if(((match & mask) + 1) & (mask + 1)) {
      if(hashmap.update_bucket_at_slot(i2, slot, key, val)) {

        table_->increment_even(i2);
        if(i1 != i2) {
          table_->increment_even(i1);
        }
        return true;

      }
    }
    match >>= 12;
  }

  table_->increment_even(i2);
  if(i1 != i2) {
    table_->increment_even(i1);
  }

  write_lock.lock();
  if(atomic_load(&victim_.used) && (key == victim_.key)) {
    victim_.val = val;
    write_lock.unlock();
    return true;
  }
  write_lock.unlock();

  return insert_impl(key, val, i1, i2, tag, tag_hash);

}

}  // namespace cuckoofilter
#endif  // CUCKOO_FILTER_CUCKOO_FILTER_H_
