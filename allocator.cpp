#include <iostream>
#include <cstddef>

constexpr size_t HEAP_SIZE = 1024 * 1024; // 1 MB fake heap
alignas(16) static char heap[HEAP_SIZE];

struct BlockHeader{
    size_t size;
    bool free;
    BlockHeader* next;
};

BlockHeader* heap_start = reinterpret_cast<BlockHeader*>(heap);

void init_heap(){
    heap_start->size = HEAP_SIZE - sizeof(BlockHeader);
    heap_start->free = true;
    heap_start->next = nullptr;
}

void* my_alloc(size_t size){
    size  = (size + 7) & ~static_cast<size_t>(7);

    BlockHeader* current = heap_start;

    while(current != nullptr){
        if(current->free && current->size >= size){
            // enough left over to carve out a new free block?
            if(current->size >= size + sizeof(BlockHeader) + 8){
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

void my_free(void* ptr){
    if(ptr == nullptr){
        return;
    }
    BlockHeader* header = reinterpret_cast<BlockHeader*>(ptr) - 1;
    header->free = true;
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
}