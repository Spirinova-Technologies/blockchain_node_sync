#include <iostream>
#include <string>
#include <list>
#include <vector>
#include <map>
#include <set>
#include <unordered_map>
#include <cstdlib>
#include <sstream>
#include <ctime>
#include <thread>         
#include <chrono>         

// Play around with these values to test different scenarios

#define MAX_BLOCKS 100
#define THREAD_COUNTS 5
#define RESPONSE_THRESHOLD 5
#define RANGE 4

struct Block {
	uint64_t height;
	std::string BlockID;
	Block(uint64_t h, std::string id):height(h),BlockID(id) {}
	bool operator <(Block& b) {
		return height<b.height;
	}
};


class RangeResponseProcessor {
	uint64_t n;
	uint64_t s;
	uint64_t h;
	// Thread-pool
	std::vector<std::thread> thread_store;
	// Lock free composite data structure for all the threads, where each thread gets its own hash map where it stores the
	// number responses for each block height, this is consolidated by main thread periodically. 
	std::unordered_map<std::thread::id, std::unordered_map<uint64_t, uint64_t> > height_count_map_map;
	
	public:
		RangeResponseProcessor(int n, int s):n(n),s(s), h(0){}
		// disabling copy constructor and assignment operator
		RangeResponseProcessor(const RangeResponseProcessor& rpc);
		RangeResponseProcessor& operator=(const RangeResponseProcessor& rpc);

		std::vector<std::thread>& getThreadStore() { return thread_store; }

		void DebugPrint(std::thread::id t_id) {
			std::unordered_map<uint64_t, uint64_t>& height_count_map = height_count_map_map[t_id];
			for (uint64_t i = 0; i<height_count_map.size(); i++) {
				std::cout<<i<<"=>"<<height_count_map[i]<<"\t";
			}
		}
		void ProcessRange(const std::vector<Block>& blocks, std::thread::id t_id) {
			std::unordered_map<uint64_t, uint64_t>& height_count_map = height_count_map_map[t_id];
			
			for (std::vector<Block>::const_iterator it = blocks.begin(); it != blocks.end(); it++) {
				if (height_count_map.find(it->height) == height_count_map.end()) {
					height_count_map[it->height] = 1;
					return;
				}
				if (height_count_map[it->height] > n) {
					return;
				}
				height_count_map[it->height]++;
			}
		}
		// This function returns the current active range
		std::pair<uint64_t,u_int64_t> GetActiveRange() {
			std::unordered_map<uint64_t, uint64_t> consolidated_map;
			for (std::vector<std::thread>::iterator it = thread_store.begin(); it != thread_store.end(); it++) {
				std::unordered_map<uint64_t, uint64_t>& hash_map_ref = height_count_map_map[it->get_id()];
				for (uint64_t start = h; start < MAX_BLOCKS; start++) {
					if (consolidated_map.find(start) == consolidated_map.end()) {
						consolidated_map[start] = 0;
					}
					if (hash_map_ref.find(start) == hash_map_ref.end()) {
						continue;
					}
					consolidated_map[start] = consolidated_map[start] + hash_map_ref[start];
				} 
			}
			for (uint64_t start = h; start < MAX_BLOCKS; start++) {
				if (consolidated_map[start] >= n) {
					h++;
					for (std::vector<std::thread>::iterator it = thread_store.begin(); it != thread_store.end(); it++) {
						std::unordered_map<uint64_t, uint64_t>& hash_map_ref = height_count_map_map[it->get_id()];
						hash_map_ref.erase(start);
					}
				} else {
					break;
				}
		    } 
			return std::make_pair(h,h+s);
		}
};

void thread_func(RangeResponseProcessor& RRP, std::vector<Block>& blocks) {
	while (true) {
		srand(time(0));
		uint64_t start =  rand()%MAX_BLOCKS;
		if (start > 90) {
			start = start - 10;
		}
		std::vector<Block> tmpBlocks;
		for (uint64_t h=start; h<start+10;h++) {
			tmpBlocks.push_back(blocks[h]);
		}	
		RRP.ProcessRange(tmpBlocks, std::this_thread::get_id());
		//RRP.DebugPrint(std::this_thread::get_id());
	}
}
int main()
{
	RangeResponseProcessor RRP(RESPONSE_THRESHOLD, RANGE);
	std::vector<Block> blocks;
	
	for (uint64_t h=0; h<MAX_BLOCKS;h++) {
		std::stringstream ss;
		std::string tmp;
		ss << rand();
		ss >> tmp; 
		Block b(h, tmp);
		blocks.push_back(b);
	}
	std::vector<std::thread>& thread_vector_ref = RRP.getThreadStore();
	for (uint64_t i=0; i<THREAD_COUNTS; i++) {
		thread_vector_ref.push_back(std::thread(thread_func, std::ref(RRP), std::ref(blocks)));
	}

	while (true) {
		std::pair<uint64_t, uint64_t> activeRange = RRP.GetActiveRange();
		//std::this_thread::sleep_for (std::chrono::milliseconds(10));
		std::cout<<"["<<activeRange.first<<","<<activeRange.second<<"]"<<std::endl;
		std::cout<<"\n\n\n"<<std::endl;
	}
}