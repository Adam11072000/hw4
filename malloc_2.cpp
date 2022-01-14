//
// Created by student on 1/13/22.
//
#include <unistd.h>
#include <cstring>
#include <cmath>

const long int MAX_SIZE = pow(10, 8);

struct MetaData{
    size_t size;
    bool is_free;
    MetaData* prev;
    MetaData* next;
};

MetaData* heapHead = NULL;
MetaData* heapTail = NULL; // last element
int count = 0;
int free_blocks = 0;



void* smalloc(size_t size){
    if(size > MAX_SIZE || size == 0){
        return NULL;
    }
    void* out = NULL;
    MetaData* it = heapHead;
    bool found_free_space= false;

    if(count == 0){
        heapHead = (MetaData*)(sbrk(intptr_t (sizeof(MetaData) + size)));
        if(heapHead == (void*)(-1)){
            exit(1);
        }
        heapHead->size = size;
        heapHead->prev = NULL;
        heapHead->next = NULL;
        heapHead->is_free = false;
        heapTail = heapHead;
        out = (void*) (heapHead+sizeof(MetaData));
    } else{
        while(it != heapTail){
            if(it->size >= size && it->is_free){
                it->is_free = false;
                out = it + sizeof(MetaData);
                found_free_space = true;
                free_blocks--;
            }
            it = it->next;
        }
        if(!found_free_space){
            heapTail->next = (MetaData*)(sbrk(intptr_t (sizeof(MetaData) + size)));
            if(heapTail == (void*)(-1)){
                exit(1);
            }
            heapTail->next->prev = heapTail;
            heapTail = heapTail->next;
            heapTail->next = NULL;
            heapTail->size = size;
            heapTail->is_free = false;
            out = (void*)(heapTail + sizeof(MetaData));
        }
    }
    count++;
    return out;
}

void* scalloc(size_t num, size_t size){
    if(size == 0 || size*num > MAX_SIZE){
        return NULL;
    }
    void* out = smalloc(size*num);
    if(out == NULL){
        return NULL;
    }
    std::memset(out,0, size*num); // check ascii
    return out;
}

void sfree(void* p){
    if(p == NULL){
        return;
    }
    MetaData* it = heapHead;
    while(it){
        if((void*)(it + sizeof(MetaData)) == p){
            it->is_free = true;
            p = NULL;
            free_blocks++;
            break;
        }
    }
}

void* srealloc(void* oldp, size_t size){
    if(size == 0 or size > MAX_SIZE){
        return NULL;
    }
    MetaData* it = heapHead;
    void* out;
    while(it){
        if((void*)(it+sizeof(MetaData)) == oldp){
            if(it->size >= size){
                return oldp;
            }
            it->is_free = true;
            break;
        }
        it = it->next;
    }
    out = smalloc(size);
    if(out == NULL){
        return NULL;
    }
    memcpy(out, oldp, size);
    return out;
}

size_t _num_free_blocks(){
    return free_blocks;
}

size_t _num_free_bytes(){
    MetaData* it = heapHead;
    size_t res = 0;
    while(it){
        if(it->is_free){
            res += it->size;
        }
        it = it->next;
    }
    return res;
}

size_t _num_allocated_blocks(){
    return count;
}

size_t _num_allocated_bytes(){
    MetaData* it = heapHead;
    size_t res = 0;
    while(it){
        res += it->size;
        it = it->next;
    }
    return res;
}

size_t _num_meta_data_bytes(){
    return sizeof(MetaData)*count;
}

size_t _size_meta_data(){
    return sizeof(MetaData);
}