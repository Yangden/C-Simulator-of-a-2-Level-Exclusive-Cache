/*
Cache Simulator
Level one L1 and level two L2 cache parameters are read from file (block size, line per set and set per cache).
The 32 bit address is divided into tag bits (t), set index bits (s) and block offset bits (b)
s = log2(#sets)   b = log2(block size in bytes)  t=32-s-b
32 bit address (MSB -> LSB): TAG || SET || OFFSET

Tag Bits   : the tag field along with the valid bit is used to determine whether the block in the cache is valid or not.
Index Bits : the set index field is used to determine which set in the cache the block is stored in.
Offset Bits: the offset field is used to determine which byte in the block is being accessed.
*/

#include <algorithm>
#include <bitset>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdlib.h>
#include <string>
#include <vector>
#include <cmath>
using namespace  std;
//access state:
#define NA 0 // no action
#define RH 1 // read hit
#define RM 2 // read miss
#define WH 3 // Write hit
#define WM 4 // write miss
#define NOWRITEMEM 5 // no write to memory
#define WRITEMEM 6   // write to memory

struct config
{
    int L1blocksize;
    int L1setsize;
    int L1size;
    int L2blocksize;
    int L2setsize;
    int L2size;
};

/*********************************** ↓↓↓ Todo: Implement by you ↓↓↓ ******************************************/
// You need to define your cache class here, or design your own data structure for L1 and L2 cache

/*
A single cache block:
    - valid bit (is the data in the block valid?)
    - dirty bit (has the data in the block been modified by means of a write?)
    - tag (the tag bits of the address)
    - data (the actual data stored in the block, in our case, we don't need to store the data)
*/
struct CacheBlock
{
    // we don't actually need to allocate space for data, because we only need to simulate the cache action
    // or else it would have looked something like this: vector<number of bytes> Data; 
    bool validbit;
    bool dirtybit;
    string tag;

    /* constructor*/
    CacheBlock() : validbit(0), dirtybit(0) {} 
};

struct CacheUnit
{
    string addr;
    bool dirtybit;

    CacheUnit() : addr(32,'0'), dirtybit(0) {} 
};
/*
A CacheSet:
    - a vector of CacheBlocks
    - a counter to keep track of which block to evict next
*/
struct set
{
    // tips: 
    // Associativity: eg. resize to 4-ways set associative cache
    vector<CacheBlock> arr; 

    /*constructor*/
    set(int numways) : arr(numways) {}
    set() {}
};

/*self-design cache data structure*/
class Cache
{
    vector<set> cacheset;  // L1 cache
    vector<set> cacheset2; //L2 cache 
    int numways;  //Associativity
    int numsets;  // The number of sets
    int numways2;
    int numsets2;
    int blocksize;
    // Counters for round-robin algorithm [key]
    vector<int> counterL1;
    vector<int> counterL2;

public:
    //default constructor
    Cache () {}
    
    Cache(int blocksize, int setsize, int cachesize,
          int blocksize2, int setsize2, int cachesize2) {
        // Determine configuration of the cache
        numways = setsize;
        if (numways == 0) { // FA cache
            numsets = 1;
            numways = int(pow(2.0,int(log2(cachesize) + 10.0 - log2(blocksize))));
        } else {
            numsets = int(pow(2.0,int(log2(cachesize) + 10.0 - log2(blocksize) - log2(numways))));
        }
        
        this->blocksize = blocksize;
        
        // init cache data struct
        cacheset.resize(numsets,set(numways));

        numways2 = setsize2;
        if (numways2 == 0) {
            numsets2 = 1;
            numways2 = int(pow(2.0,int(log(cachesize2) + 10 - log2(blocksize2))));
        } else {
            numsets2 = int(pow(2.0,log2(cachesize2) + 10.0 - log2(blocksize2) - log2(numways2)));
        }
       
        cacheset2.resize(numsets2,set(numways2));
        
        //initialize counters for round-robin algorithm 
        counterL1.resize(numsets,0);
        counterL2.resize(numsets2,0);
    }
    
    vector<string> parse(string addr,int sets) {
        vector<string> blockelements;
        string index;
        /* parsing :: determine range*/
        int offset_range = int(log2(blocksize));
        int index_range = int(log2(sets));
        int tag_range = 32 - index_range - offset_range;
        /* parsing :: split*/
        string tag = addr.substr(0,tag_range);
        if (index_range != 0) {  // not a FA cache
            index = addr.substr(tag_range,index_range);
        } else {
            index = ""; //empty string - FA cache
        }
        string offset = addr.substr(addr.length() - offset_range);
        /*assemble*/
        blockelements.push_back(tag);
        blockelements.push_back(index);
        blockelements.push_back(offset);

        return blockelements;
    }

    bool if_empty(vector<set> &cache_set,string addr,
               bool dirtybit, int ways, int sets) {
        vector<string> blockelements; //store the three elements of blocks
        blockelements = parse(addr,sets);
        string tag = blockelements[0];
        string index = blockelements[1];
        string offset = blockelements[2];
        int index_toint;

        /* determine if it is full*/
        if (index.empty()) {
            index_toint = 0;
        } else {
            index_toint = stoi(index,0,2);
        }
        set &index_set = cache_set.at(index_toint);
        bool avail = false;
        for (int i = 0; i < index_set.arr.size(); i++) {
            if (index_set.arr[i].validbit != 1) {
                avail = true;  //empty space
                // place the data
                index_set.arr[i].tag = tag;
                index_set.arr[i].validbit = 1;
                index_set.arr[i].dirtybit = dirtybit;
                break;
            }
        }

        return avail;
    }

    bool if_emptyL2(CacheUnit u) {
        vector<string> blockelements;
        blockelements = parse(u.addr,numsets2);
        string tag = blockelements[0];
        string index = blockelements[1];
        string offset = blockelements[2];
        int index_toint;

        if (index.empty()) {
            index_toint = 0;
        } else {
            index_toint = stoi(index,0,2);
        }

        set &index_set = cacheset2.at(index_toint);
        bool avail = false;
        // mind - check dirty bit first 
        for (CacheBlock &block : index_set.arr) {
            if (block.dirtybit == 0) { //available space - write and set it to one
                avail = true;
                block.tag = tag;
                block.validbit = 1;
                block.dirtybit = u.dirtybit;
                break;
            }
        }
        return avail;
    }

    auto eviction(vector<set> &cache_set, vector<int> &counter, bool dirtybit,
                    int ways, int sets,string addr)
    {
        CacheUnit u;
        // parsing : correct configuration
        vector<string> blockelements;
        blockelements = parse(addr,sets);
        string tag = blockelements[0];
        string index = blockelements[1];
        string offset = blockelements[2];
        int index_toint;

        if (index.empty()) {
            index_toint = 0;
        } else {
            index_toint = stoi(index,0,2);
        }
        set index_set = cache_set[index_toint];
        int pos = counter[index_toint]; //locate the position
        /* round robin algorithm */
        if (counter[index_toint] == ways - 1) {
            counter[index_toint] = 0;
        } else {
            counter[index_toint]++;
        }

        /* get the evict block and place the data*/
        CacheBlock b = index_set.arr[pos];
        u.dirtybit = b.dirtybit;
        if (index.empty()) {
            u.addr = b.tag + offset;
        } else {
            u.addr = b.tag + index + offset;
        }

        /* place the data - no need to modify the valid bit*/
        cache_set[index_toint].arr[pos].dirtybit = dirtybit;
        cache_set[index_toint].arr[pos].tag = tag;
      
        return u;

    }

    auto evictL2(CacheUnit u) {
        vector<string> blockelements;
        blockelements = parse(u.addr,numsets2);
        string tag = blockelements[0];
        string index = blockelements[1];
        string offset = blockelements[2];
        int index_toint;

        if (index.empty()) {
            index_toint = 0;
        } else {
            index_toint = stoi(index,0,2);
        }
        // pay attention to the reference
        set &index_set = cacheset2.at(index_toint);
        // evict position
        int pos = counterL2[index_toint];
        // run the round-robin algorithm
        if (counterL2[index_toint] == numways2 - 1) {
            counterL2[index_toint] = 0;
        } else {
            counterL2[index_toint]++;
        }
        
        // locate the evict block and place the data
        index_set.arr.at(pos).tag = tag;
        index_set.arr.at(pos).dirtybit = u.dirtybit;
        index_set.arr.at(pos).validbit = 1;  // ignore the 'evicted data'- just model the actions
    }
    
    auto readL1(string addr) {
        set index_set;
        string index;
       /* parsing :: determine range*/
        int offset_range = int(log2(blocksize));
        int index_range = int(log2(numsets));
        int tag_range = 32 - index_range - offset_range;
        /* parsing :: split*/
        string tag = addr.substr(0,tag_range);
        if (index_range == 0) {
             index = "";
        } else {
             index = addr.substr(tag_range,index_range);
        }
        
        string offset = addr.substr(addr.length() - offset_range);

        /* hit or miss*/
        if (index.empty()) { //FA cache
           index_set = cacheset.at(0);
        } else {
           index_set = cacheset.at(stoi(index,0,2)); // loacate the set
        }
        
        // iterate through blocks 
        bool hit = false;
        for (CacheBlock block : index_set.arr) {
            if (block.validbit == 1 && block.tag.compare(tag) == 0) {
                hit = true;
                break;
            }
        }

        if (hit) {
            return RH;
        } else {
            return RM;
        }
         
    }

    auto readL2(string addr) {
        vector<int> states; //store L2state and writememory_state
        bool hit_dirtybit = 0;  //Possible to be placed in L1, copy dirty bit
        bool empty = false;  //'if_evict flag', determine whether trigger events in upper level 
        CacheUnit u;  // store the evicted block
        int index_toint;   // relate to wheter it is a FA cache

        /* parsing :: determine range*/
        int offset_range = int(log2(blocksize));
        int index_range = int(log2(numsets2));
        int tag_range = 32 - index_range - offset_range;
        /* parsing :: split*/
        string tag = addr.substr(0,tag_range);
        string index = addr.substr(tag_range,index_range);
        string offset = addr.substr(addr.length() - offset_range);

        /* hit or miss*/
        if (index.empty()) {
            index_toint = 0;
        } else {
            index_toint = stoi(index,0,2);
        }
        set &index_set = cacheset2.at(index_toint); // loacate the set
        // iterate through blocks 
        bool hit = false;
        bool avail = false;
        for (CacheBlock &block : index_set.arr) {
            if (block.validbit == 1 && block.tag.compare(tag) == 0) {
                hit = true;
                hit_dirtybit = block.dirtybit;
                block.validbit = 0;   //invalidate
                block.dirtybit = 0;
                break;
            }
        }
        //determine L2state
        if (hit) {
            states.push_back(RH);
            empty = if_empty(cacheset,addr,hit_dirtybit,numways,numsets); 
            if (empty) { // no eviction
                states.push_back(NOWRITEMEM);
            } else {
                u = eviction(cacheset,counterL1,hit_dirtybit,numways,numsets,addr);
                /* L2 conditions*/
                empty = if_emptyL2(u);
                if (empty) {
                    states.push_back(NOWRITEMEM);
                } else {
                    states.push_back(WRITEMEM);
                    evictL2(u);
                }
            }
        } else {
            states.push_back(RM);
            empty = if_empty(cacheset,addr,0,numways,numsets);
            if (empty) {
                states.push_back(NOWRITEMEM);
            } else {
                u = eviction(cacheset,counterL1,0,numways,numsets,addr);
                empty = if_emptyL2(u);
                if (empty) {
                    states.push_back(NOWRITEMEM);
                } else {
                    states.push_back(WRITEMEM);
                    evictL2(u);
                }
            }
        }

        return states;
    }
    
    bool write(vector<set> &cache_set,string addr,int sets) {
        vector<string> blockelements;
        bool hit = false;
        //parse
        blockelements = parse(addr,sets);
        string tag = blockelements[0];
        string index = blockelements[1];
        string offset = blockelements[2];
        int index_toint;

        if (index.empty()) {
            index_toint = 0;
        } else {
            index_toint = stoi(index,0,2);
        }

        // locate the set
        set &index_set = cache_set.at(index_toint);
        for (CacheBlock& block : index_set.arr) {
            if (block.validbit == 1 && block.tag == tag) {  //hit
                hit = true;
                block.dirtybit = 1;
                break;
            }
        }

        return hit;
    }
    auto writeL1(string addr){
        bool hit = false;
        hit = write(cacheset,addr,numsets);
        if (hit) {
            return WH;
        } else {
            return WM;
        }
    }

    auto writeL2(string addr){
        vector<int> states;
        bool hit = false;
       
        hit = write(cacheset2,addr,numsets2);
        if (hit) {
            states.push_back(WH);
            states.push_back(NOWRITEMEM);
        } else {
            states.push_back(WM);
            states.push_back(WRITEMEM);
        }

        return states;
    }
    

};
/*
// Tips: another example:
class CacheSystem
{
    Cache L1;
    Cache L2;
public:
    int L1AcceState, L2AcceState, MemAcceState;

    CacheSystem() {}
    
    CacheSystem(int L1blocksize,int L1setsize,int L1cachesize,
                int L2blocksize,int L2setsize,int L2cachesize) 
    {
        L1 = Cache(L1blocksize,L1setsize,L1cachesize);
        L2 = Cache(L2blocksize,L2setsize,L2cachesize);
        L1AcceState = L2AcceState = MemAcceState = NA;
    }

    auto read(auto addr){
        L1AcceState = L1.readL1(addr);
        if (L1AcceState == RH) {
            L2AcceState = NA;
            MemAcceState = NOWRITEMEM;
        } else {  // Read Miss in L1
            
        }
    }
    auto write(auto addr){

    }
};
*/
/**
class Cache
{
    set * CacheSet;
public:
    auto read_access(auto addr){};
    auto write_access(auto addr){};
    auto check_exist(auto addr){};
    auto evict(auto addr){};
};*/
/*********************************** ↑↑↑ Todo: Implement by you ↑↑↑ ******************************************/





int main(int argc, char *argv[])
{

    config cacheconfig;
    ifstream cache_params;
    string dummyLine;
    cache_params.open(argv[1]);
    while (!cache_params.eof())                   // read config file
    {
        cache_params >> dummyLine;                // L1:
        cache_params >> cacheconfig.L1blocksize;  // L1 Block size
        cache_params >> cacheconfig.L1setsize;    // L1 Associativity
        cache_params >> cacheconfig.L1size;       // L1 Cache Size
        cache_params >> dummyLine;                // L2:
        cache_params >> cacheconfig.L2blocksize;  // L2 Block size
        cache_params >> cacheconfig.L2setsize;    // L2 Associativity
        cache_params >> cacheconfig.L2size;       // L2 Cache Size
    }
    ifstream traces;
    ofstream tracesout;
    string outname;
    outname = string(argv[2]) + ".out";
    traces.open(argv[2]);
    tracesout.open(outname.c_str());
    string line;
    string accesstype;     // the Read/Write access type from the memory trace;
    string xaddr;          // the address from the memory trace store in hex;
    unsigned int addr;     // the address from the memory trace store in unsigned int;
    bitset<32> accessaddr; // the address from the memory trace store in the bitset;




/*********************************** ↓↓↓ Todo: Implement by you ↓↓↓ ******************************************/
    // Implement by you:
    // initialize the hirearch cache system with those configs
    // probably you may define a Cache class for L1 and L2, or any data structure you like
    if (cacheconfig.L1blocksize!=cacheconfig.L2blocksize){
        printf("please test with the same block size\n");
        return 1;
    }

    // cache c1(cacheconfig.L1blocksize, cacheconfig.L1setsize, cacheconfig.L1size,
    //          cacheconfig.L2blocksize, cacheconfig.L2setsize, cacheconfig.L2size);
   
    Cache c(cacheconfig.L1blocksize,cacheconfig.L1setsize,cacheconfig.L1size,
            cacheconfig.L2blocksize,cacheconfig.L2setsize,cacheconfig.L2size);

    int L1AcceState = NA; // L1 access state variable, can be one of NA, RH, RM, WH, WM;
    int L2AcceState = NA; // L2 access state variable, can be one of NA, RH, RM, WH, WM;
    int MemAcceState = NOWRITEMEM; // Main Memory access state variable, can be either NA or WH;
    vector<int> states; // store states returned by 'L2'

    if (traces.is_open() && tracesout.is_open())
    {
        while (getline(traces, line))
        { // read mem access file and access Cache

            istringstream iss(line);
            if (!(iss >> accesstype >> xaddr)){
                break;
            }
            stringstream saddr(xaddr);
            saddr >> std::hex >> addr;
            accessaddr = bitset<32>(addr);
            
            
            // access the L1 and L2 Cache according to the trace;
            if (accesstype.compare("R") == 0)  // a Read request
            {
                // Implement by you:
                //   read access to the L1 Cache
                L1AcceState = c.readL1(accessaddr.to_string());
                //   and then L2 (if required),
                if (L1AcceState == RH) {
                    L2AcceState = NA;
                    MemAcceState = NOWRITEMEM;
                } else {  //Access L2
                    states = c.readL2(accessaddr.to_string());
                    L2AcceState = states[0];
                    MemAcceState = states[1];
                }
                //   update the access state variable;
                //   return: L1AcceState L2AcceState MemAcceState
                
                // For example:
                // L1AcceState = cache.readL1(addr); // read L1
                // if(L1AcceState == RM){
                //     L2AcceState, MemAcceState = cache.readL2(addr); // if L1 read miss, read L2
                // }
                // else{ ... }
            }
            else{ // a Write request
                // Implement by you:
                //   write access to the L1 Cache, or L2 / main MEM,
                L1AcceState = c.writeL1(accessaddr.to_string());
                //   update the access state variable;
                //   return: L1AcceState L2AcceState
                if (L1AcceState == WH) {
                    L2AcceState = NA;
                    MemAcceState = NOWRITEMEM;
                } else {
                    states = c.writeL2(accessaddr.to_string());
                    L2AcceState = states[0];
                    MemAcceState = states[1];
                }
                // For example:
                // L1AcceState = cache.writeL1(addr);
                // if (L1AcceState == WM){
                //     L2AcceState, MemAcceState = cache.writeL2(addr);
                // }
                // else if(){...}
            }
/*********************************** ↑↑↑ Todo: Implement by you ↑↑↑ ******************************************/




            // Grading: don't change the code below.
            // We will print your access state of each cycle to see if your simulator gives the same result as ours.
            tracesout << L1AcceState << " " << L2AcceState << " " << MemAcceState << endl; // Output hit/miss results for L1 and L2 to the output file;
            L1AcceState = L2AcceState = MemAcceState = NA; // initialization for the new state 
        }
        traces.close();
        tracesout.close();
    }
    else
        cout << "Unable to open trace or traceout file ";

    return 0;
}
