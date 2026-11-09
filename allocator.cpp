#include<iostream>
#include <cstddef>
#include <cstdint>

constexpr size_t HEAP_SIZE = 1024 * 1024;   // 1 MB fake heap
constexpr size_t ALIGNMENT = 16;
constexpr size_t MAGIC_FREE = 0xF4EEB10C;   //stamped on a block when it is free
constexpr size_t MAGIC_USED = 0xA110C8ED;   //stamped on a block when it is in use

void (*log_fn)(const char*) = nullptr;

void log(const char* msg){
    if(log_fn != nullptr){
        log_fn(msg);
    }
}

struct alignas(ALIGNMENT) BlockHeader{
    size_t size;
    size_t magic;
    bool free;
    BlockHeader* next;
    BlockHeader* prev;
};

static_assert(sizeof(BlockHeader) % ALIGNMENT == 0, "header must keep payloads aligned");

char* heap_base = nullptr;      //where the region begins
size_t heap_size = 0;         //size in raw bytes
BlockHeader* heap_start = nullptr;  //creation of first block

constexpr size_t align_up(size_t n){
    return (n + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
}

constexpr size_t align_down(size_t n){
    return n & ~(ALIGNMENT - 1);
}

//allows the computer to manage its own memory
bool init_heap(void* start, size_t size){       //start = the region where it starts, size = size in bytes
   if (start == nullptr){
       return false;    //renders the region unusable(nothing changes)
   }

    //the region doesnt garuntee a 16-byte boundry, but a block must have one
    //so we skip to the first aligned address
    uintptr_t raw = reinterpret_cast<uintptr_t>(start);
    uintptr_t aligned = align_up(raw);
    size_t skipped = aligned - raw;     //bytes thrown away to reach the boundry

    if (size <= skipped){
        return false;       //nothing remaining afer skipping
    }
    size -= skipped;
    size = align_down(size);       //the end must land on a boundry

    // The region must fit a header plus at least one aligned chunk of payload
    if (size < sizeof(BlockHeader) + ALIGNMENT){
        return false;
    }

    heap_base = reinterpret_cast<char*>(aligned);
    heap_size = size;
    heap_start = reinterpret_cast<BlockHeader*>(heap_base);

    //region starts out as 1 big free block
    heap_start->size = heap_size - sizeof(BlockHeader);
    heap_start->free = true;
    heap_start->magic = MAGIC_FREE;
    heap_start->next = nullptr;
    heap_start->prev = nullptr;

    return true;
}

void* my_alloc(size_t size){
    if (size == 0 || size > heap_size){
        return nullptr;
    }
    size = align_up(size);

    BlockHeader* current = heap_start;

    while(current != nullptr){
        if(current->free && current->size >= size){
            // checks to see if enough left over to carve out a new free block
            if(current->size >= size + sizeof(BlockHeader) + ALIGNMENT){
                BlockHeader* new_block = reinterpret_cast<BlockHeader*>(
                reinterpret_cast<char*>(current + 1) + size);
                new_block->size = current->size - size - sizeof(BlockHeader);
                new_block->free = true;
                new_block->magic = MAGIC_FREE;  //every new block that comes out of an already existing block gets the 'free' stamp
                new_block->next = current->next;
                //make the current block the previous block;
                new_block->prev = current;
                if (new_block->next != nullptr){        //if next and current are free, combine them into 1 block
                    new_block->next->prev = new_block;
                }

                current->size = size;
                current->next = new_block;
            }
            current->free = false;
            current->magic = MAGIC_USED;    //when the bool(free) is false, the used stamp is applied
            return reinterpret_cast<void*>(current + 1);
        }
        current = current->next;
    }
    return nullptr; // no block big enough found
}

void coalesce(BlockHeader* block){      //merges 'block' nearest neighbors if they are frees
    if (block->next != nullptr && block->next->free){     //if block->next exists and it is free
        block->size += sizeof(BlockHeader) + block->next->size;     // block gains the next block's payload AND its header (the header becomes normal space)
        //unlink block->next as it has become 1 block
        block->next = block->next->next;

        if (block->next != nullptr){
            block->next->prev = block;
        }
    }
    //merge with previous block
    if (block->prev != nullptr && block->prev->free){
        block->prev->size += sizeof(BlockHeader) + block->size;      // previous block gains block's payload AND its header (the header becomes normal space)
        block->prev->next = block->next;    // skips over block so it is no longer in the list

        // the block after the swallowed one must now point back at the previous block
        if (block->next != nullptr){
            block->next->prev = block->prev;
        }
    }
}

void my_free(void* ptr){
    if(ptr == nullptr){
        return;
    }
    //checks for valid pointer
    char* p = reinterpret_cast<char*>(ptr);
    if (p < heap_base + sizeof(BlockHeader) || p >= heap_base + heap_size){
        log("my_free: invalid pointer\n");
        return;
    }
    BlockHeader* header = reinterpret_cast<BlockHeader*>(ptr) -1;

    //check #2: is the header already freed?
    if (MAGIC_FREE == header->magic){
        log("my_free: double free detected\n");
        return;
    }

    //check #3: not stamped?
    if (header->magic != MAGIC_USED){
        log("my_free: invalid pointer\n");
        return;
    }

    //if all checks are passed: this is a real, in-use block, free it
    header->free = true;
    header->magic = MAGIC_FREE;
    coalesce(header);   //coalesce(BlockHeader* block)
}

// The test program's way of printing: just forwards the message to output.
void test_log(const char* msg){
    std::cout << msg;
}

int main(){
    log_fn = test_log;

    alignas(ALIGNMENT) static char test_heap[HEAP_SIZE];

    //hand over the region to the allocator, instead of manually handling it
    if (!init_heap(test_heap, sizeof(test_heap))){
        std::cout << "init_heap failed\n";
        return 1; //stop the prgoram, no heap is available to test
    }

    int* a = static_cast<int*>(my_alloc(sizeof(int)));
    *a = 42;
    std::cout << "a = " << *a << "\n";

    my_free(a);

    int* b = static_cast<int*>(my_alloc(sizeof(int)));
    std::cout << "block reused: " << (a == b ? "yes" : "no") << "\n";

    int* c = static_cast<int*>(my_alloc(sizeof(int)));
    std::cout << "second alloc: " << (c == nullptr ? "failed" : "worked") << "\n";
    std::cout << "b and c are different: " << (b != c ? "yes" : "no") << "\n";

    my_free(b);
    my_free(c);

    void* d = my_alloc(1);
    std::cout << "16-aligned: " << (reinterpret_cast<uintptr_t>(d) % 16 == 0 ? "yes" : "no") << "\n";
    std::cout << "zero alloc: " << (my_alloc(0) == nullptr ? "rejected" : "allowed") << "\n";
    my_free(d);

    void* e  = my_alloc(32);
    my_free(e);     // normal free: fine, no message
    my_free(e);     // second free of the same pointer
    int x = 0;
    my_free(&x);    // pointer to a local variable, not in our heap

    //coalescing test: allocate 3 blocks, free them all
    //then ask for a whole heap in one go 
    void* p1 = my_alloc(100);
    void* p2 = my_alloc(100);
    void* p3 = my_alloc(100);

    my_free(p1);
    my_free(p2);
    my_free(p3);
    void* big = my_alloc(heap_size - sizeof(BlockHeader));
    std::cout << "big alloc after freeing all: " << (big != nullptr ? "worked" : "failed") << "\n";
}