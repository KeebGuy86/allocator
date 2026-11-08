#include <iostream>
#include <cstddef>
#include <cstdint>

constexpr size_t HEAP_SIZE = 1024 * 1024; // 1 MB fake heap
constexpr size_t ALIGNMENT = 16;
alignas(ALIGNMENT) static char heap[HEAP_SIZE];

struct alignas(ALIGNMENT) BlockHeader{
    size_t size;
    bool free;
    BlockHeader* next;
};

static_assert(sizeof(BlockHeader) % ALIGNMENT == 0, "header must keep payloads aligned");

BlockHeader* heap_start = reinterpret_cast<BlockHeader*>(heap);

constexpr size_t align_up(size_t n){
    return (n + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
}

void init_heap(){
    heap_start->size = HEAP_SIZE - sizeof(BlockHeader);
    heap_start->free = true;
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
            // enough left over to carve out a new free block?
            if(current->size >= size + sizeof(BlockHeader) + ALIGNMENT){
                BlockHeader* new_block = reinterpret_cast<BlockHeader*>(
                reinterpret_cast<char*>(current + 1) + size);
                new_block->size = current->size - size - sizeof(BlockHeader);
                new_block->free = true;
                new_block->next = current->next;

                current->size = size;
                current->next = new_block;
            }
            current->free = false;
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
    BlockHeader* header = reinterpret_cast<BlockHeader*>(ptr) - 1;
    header->free = true;
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

    void* t = my_alloc(1);
    std::cout << "16-aligned: " << (reinterpret_cast<uintptr_t>(t) % 16 == 0 ? "yes" : "no") << "\n";
    std::cout << "zero alloc: " << (my_alloc(0) == nullptr ? "rejected" : "allowed") << "\n";
    my_free(t);
 
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