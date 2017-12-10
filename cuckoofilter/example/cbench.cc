#include <iostream>
#include <ctime>
#include <vector>
#include <thread>

#include "cukoofilter.h"

using namespace std;
using cuckoofilter::CuckooFilter;


int main(int argc, char* argv[]) {

	size_t num_elements = 1000000;
	size_t num_threads = 4;
	double mutation_fraction = 0.05;
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
			mutation_fraction = 0.05;
		}
	}

	cout << "Mutation Fraction: " << mutation_fraction << "\n";

	

}