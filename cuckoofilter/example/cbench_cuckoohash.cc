
#include <iostream>
#include <string>
#include <sstream>
#include <ctime>
#include <vector>
#include <thread>
#include <mutex>
#include <sys/time.h>
#include <cstdlib>
#include <ctime>
#include <libcuckoo/cuckoohash_map.hh>

using namespace std;

::cuckoohash_map<size_t, uint64_t> *chash;
size_t num_elements = 100000000;
size_t num_threads = 4;
size_t mutation_fraction = 5;
std::mutex mutate_lock;

void cbench_insert(size_t num_elements) {
	for(size_t i = 0; i < num_elements; i++) {
		chash->insert(i, i);
	}
}

void cbench_search() {
	size_t num_e = 2 * num_elements;
	size_t mutation_frac = mutation_fraction;
	uint64_t val; 
	bool ret_val;

	for (size_t i = 0; i < num_e; i++) {
		if (mutation_frac) {
			ret_val = chash->find(i, val);
			if ((ret_val) && ((rand() % 100) <= mutation_frac)) {
				ret_val = chash->erase(i);
			}
		} else {
			ret_val = chash->find(i, val);
		}

		(void) ret_val;
	}
}

void cbench_delete() {
	size_t num_e = num_elements;
	bool ret_val;
	for(size_t i = 0; i < num_e; i++) {
		if (chash->contains(i)) {
			ret_val = chash->erase(i);
			(void) ret_val;
		}
	}
}


int main(int argc, char* argv[]) {

	struct timeval start, end;
	srand (time(NULL));

	if(argc >= 2) {
		stringstream input_string(argv[1]);
		input_string >> num_elements;
		if(input_string.fail()) {
			std::cout << "Invalid number: " << argv[1] << "\n";
			num_elements = 1000000;
		}
	}

	cout << "Number of elements: " << num_elements << "\n";

	if(argc >= 3) {
		stringstream input_string(argv[2]);
		input_string >> num_threads;
		if(input_string.fail()) {
			std::cout << "Invalid number: " << argv[2] << "\n";
			num_threads = 4;
		}
	}

	cout << "Number of threads: " << num_threads << "\n";

	if(argc >= 4) {
		stringstream input_string(argv[3]);
		input_string >> mutation_fraction;
		if(input_string.fail()) {
			std::cout << "Invalid number: " << argv[3] << "\n";
			mutation_fraction = 5;
		}
	}

	cout << "Mutation Fraction: " << mutation_fraction << "\n";


	chash = new ::cuckoohash_map<size_t, uint64_t>(num_elements);

	
	// vector<thread> threads;
	// gettimeofday(&start, NULL);
	// for(int i = 0; i < num_threads; i++) {
	// 	threads.push_back(thread(cbench_search));
	// }

	// for (int i = 0; i < num_threads; ++i) {
	// 	threads[i].join();
	// }
	// gettimeofday(&end, NULL);

	// cout << "Number of elements remaining: " << chash->size() << "\n";
	// cout << "Time = " << (((end.tv_sec - start.tv_sec)*1e6) + (end.tv_usec - start.tv_usec)) / 1e6 << "\n";

	gettimeofday(&start, NULL);
	cbench_insert(num_elements);
	gettimeofday(&end, NULL);

	cout << "Number of elements inserted: " << chash->size() << "\n";
	cout << "Time = " << (((end.tv_sec - start.tv_sec)*1e6) + (end.tv_usec - start.tv_usec)) / 1e6 << "\n";

	//vector<pair < size_t, uint64_t> > elements;


	gettimeofday(&start, NULL);
	//chash->iterate_all(elements);
	//m::cuckoohash_map<size_t, uint64_t>::iterator it = chash->begin();
	gettimeofday(&end, NULL);

	//cout << "Iterated over: " << elements.size() << "\n";
	cout << "Time = " << (((end.tv_sec - start.tv_sec)*1e6) + (end.tv_usec - start.tv_usec)) / 1e6 << "\n";

	//elements.clear();


	vector<thread> threads;
	gettimeofday(&start, NULL);
	for(int i = 0; i < num_threads; i++) {
		threads.push_back(thread(cbench_search));
	}

	for (int i = 0; i < num_threads; ++i) {
		threads[i].join();
	}
	gettimeofday(&end, NULL);

	cout << "Number of elements remaining: " << chash->size() << "\n";
	cout << "Time = " << (((end.tv_sec - start.tv_sec)*1e6) + (end.tv_usec - start.tv_usec)) / 1e6 << "\n";

	gettimeofday(&start, NULL);
	cbench_delete();
	gettimeofday(&end, NULL);

	cout << "Number of elements after delete: " << chash->size() << "\n";
	cout << "Time = " << (((end.tv_sec - start.tv_sec)*1e6) + (end.tv_usec - start.tv_usec)) / 1e6 << "\n";

	delete chash;


}