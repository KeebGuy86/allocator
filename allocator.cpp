#include <iostream>
#include <cstddef>

constexpr size_t HEAP_SIZE = 1024 * 1024; // 1 MB fake heap
static char heap[HEAP_SIZE];

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
    BlockHeader* current = heap_start;

    while(current != nullptr){
        if(current->free && current->size >= size){
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
}