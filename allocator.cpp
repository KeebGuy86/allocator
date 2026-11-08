#include <iostream>
#include <cstddef>
#include <cstdint>

constexpr size_t HEAP_SIZE = 1024 * 1024;   // 1 MB fake heap
constexpr size_t ALIGNMENT = 16;
constexpr size_t MAGIC_FREE = 0xF4EEB10C;   //stamped on a block when it is free
constexpr size_t MAGIC_USED = 0xA110C8ED;   //stamped on a block when it is in use

alignas(ALIGNMENT) static char heap[HEAP_SIZE];

struct alignas(ALIGNMENT) BlockHeader{
    size_t size;
    size_t magic;
    bool free;
    BlockHeader* next;
};

static_assert(sizeof(BlockHeader) % ALIGNMENT == 0, "header must keep payloads aligned");

BlockHeader* heap_start = reinterpret_cast<BlockHeader*>(heap);

constexpr size_t align_up(size_t n){
    return (n + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
}

void init_heap(){
    heap_start->size = HEAP_SIZE - sizeof(BlockHeader);     //starts off as 1 big free block
    heap_start->free = true;
    heap_start->magic = MAGIC_FREE;     // applies the 'free' stamp
    heap_start->next = nullptr;
}

void* my_alloc(size_t size){
    if (size == 0 || size > HEAP_SIZE){
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

void coalesce(){
    BlockHeader* current = heap_start;
    while(current != nullptr && current->next != nullptr){
        if(current->free && current->next->free){          //checks to see if both the current block and the next block are free
            current->size += sizeof(BlockHeader) + current->next->size;    //if they are it will combine them into one big block
            current->next = current->next->next;
        }
        else{
            current = current->next;
        }
    }
}

void my_free(void* ptr){
    if(ptr == nullptr){
        return;
    }
    //checks for valid pointer
    char* p = reinterpret_cast<char*>(ptr);
    if (p < heap + sizeof(BlockHeader) || p >= heap + HEAP_SIZE){
        std::cout << "my_free: invalid pointer\n";
        return;
    }
    BlockHeader* header = reinterpret_cast<BlockHeader*>(ptr) -1;

    //check #2: is the header already freed?
    if (MAGIC_FREE == header->magic){
        std::cout << "my_free: double free detected\n";
        return;
    }

    //check #3: not stamped?
    if (header->magic != MAGIC_USED){
        std::cout << "my_free: invalid pointer\n";
        return;
    }

    //if all checks are passed: this is a real, in-use block, free it
    header->free = true;
    header->magic = MAGIC_FREE;
    coalesce();
}

int main(){
    init_heap();

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
    void* big = my_alloc(HEAP_SIZE - sizeof(BlockHeader));
    std::cout << "big alloc after freeing all: " << (big != nullptr ? "worked" : "failed") << "\n";
}