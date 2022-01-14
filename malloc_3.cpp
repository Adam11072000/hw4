//
// Created by student on 1/14/22.
//

//
// Created by student on 1/13/22.
//
#include <unistd.h>
#include <cstring>
#include <cmath>
#include <iostream>

using std::cout;
using std::endl;

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
const int hist_size = 128;
const int min_split = 128;
const size_t one_kb = 1024; // make sure not 1000
MetaData* free_hist[hist_size] = {nullptr};


static MetaData* removeFreeBlock(MetaData* target);
static void insertToHist(MetaData* metadata);
static MetaData* findAdjacentBlocks(MetaData* target_block, MetaData** to_return_upper);
static void mergeAdjacent(MetaData* target);
static MetaData* checkWilderness();
static void insertToAllocatedList(MetaData* to_insert);

static MetaData* checkWilderness() {
    MetaData* it;

    for(int i = 0; i < hist_size; i++){
        it = free_hist[i];
        while(it){
            if((it + sizeof(MetaData) + it->size) == (void*)sbrk(0)){
                removeFreeBlock(it);
                return it;
            }
            it = it->next;
        }
    }
    return NULL;
}

static void split(MetaData** to_split, size_t size){

    if((*to_split)->size - size < min_split){
        return;
    }

    if((*to_split)->size > size){
        MetaData* secondHalf = *to_split + sizeof(MetaData) + size;
        secondHalf->size = (*to_split)->size - size;
        insertToHist(secondHalf);
        (*to_split)->size = size;
    }
}

/** size is def smaller than 128kb */
static void insertToHist(MetaData* metadata){
    MetaData* it = free_hist[metadata->size / one_kb];
    MetaData* prev = free_hist[metadata->size / one_kb];

    if(!it){
        free_hist[metadata->size / one_kb] = metadata;
        metadata->next = NULL;
        metadata->prev = NULL;
        mergeAdjacent(metadata);
        return;
    }
    if(it > metadata){
        metadata->next = it;
        it->prev = metadata;
        free_hist[metadata->size / one_kb] = metadata;
        mergeAdjacent(metadata);
        return;
    }
    while(it && it < metadata){
        prev = it;
        it = it->next;
    }
    if(it != NULL){ // if here then it >= metadata
        it->prev = metadata;
    }
    metadata->prev = prev;
    prev->next = metadata;
    metadata->next = it;
    mergeAdjacent(metadata);
}

/** get the first empty space, return null if none is found */
static MetaData* findFreeSpace(size_t size){
    MetaData* it = NULL;
    size_t index = -1;
    MetaData* min = NULL;

    for(size_t i = size; i < hist_size*one_kb; i+=one_kb){
        it = free_hist[i / one_kb];
        while(it){
            if(it->size >= size && (min == NULL || it < min)){
                min = it;
                index = i;
                break; // we won't find a lower address block
            }
            it = it->next;
        }
    }
    if(min == NULL){
        return NULL;
    }
    if(min == free_hist[index / one_kb]){
        free_hist[index / one_kb] = min->next;
        if(min->next){
            min->next->prev = NULL;
        }
    }else{
        if(min->next != NULL){
            min->next->prev = min->prev;
        }
        min->prev->next = min->next;
    }
    if(min->size > size){
        split(&min, size);
    }
    return min;
}

static MetaData* findAdjacentBlocks(MetaData* target_block, MetaData** to_return_upper){
    MetaData* it;
    MetaData* to_return_lower = NULL;
    *to_return_upper = NULL;

    for(int i = 0; i < hist_size; i++){
        it = free_hist[i];
        while(it){
            if(it + it->size + sizeof(MetaData) == target_block){
                to_return_lower = it;
            }
            if(target_block + target_block->size + sizeof(MetaData) == it) {
                *to_return_upper = it;
            }
            it = it->next;
        }
    }
    return to_return_lower;
}

/**remove by pointer*/
static MetaData* removeFreeBlock(MetaData* target){
    MetaData* it;

    for(int i = 0; i < hist_size; i++){
        // check if list head
        it = free_hist[i];
        if(it == target){
            free_hist[i] = it->next;
            if(it->next){
                it->next->prev = NULL;
            }
            return it;
        }
        // def not list head
        it = it->next;
        while(it){
            if(it == target){
                it->prev->next = it->next;
                if(it->next){
                    it->next->prev = it->prev;
                }
                return it;
            }
            it = it->next;
        }
    }
    return NULL;
}

void mergeAdjacent(MetaData* target){
    MetaData* upper = NULL;
    MetaData* lower = findAdjacentBlocks(target, &upper);
    if(!lower && !upper){
        return;
    } else if(lower && !upper){
        removeFreeBlock(lower);
        removeFreeBlock(target);
        lower->size = lower->size + target->size;
        insertToHist(lower);
    } else if(!lower && upper){
        removeFreeBlock(target);
        removeFreeBlock(upper);
        target->size = target->size + upper->size;
        insertToHist(target);
    } else{
        removeFreeBlock(lower);
        removeFreeBlock(target);
        removeFreeBlock(upper);
        lower->size = lower->size + target->size + upper->size;
        insertToHist(lower);
    }
}


void print_hist(){
    for(int i = 0; i < hist_size; i++){
        MetaData* it = free_hist[i];
        if(it){
            std::cout << "this is free_hist[" << i << "]:===================" << std::endl;
        }
        while(it){
            std::cout << it->size << endl;
            it = it->next;
        }
    }
    std::cout << "======================================================================================" << endl;
}

int main(){
    MetaData* m1 = new MetaData;
    m1->size = 1;
    MetaData* m2 = new MetaData;
    m2->size = 2;
    MetaData* m3 = new MetaData;
    m3->size = 3;
    MetaData* m4 = new MetaData;
    m4->size = 1+one_kb;
    MetaData* m5 = new MetaData;
    m5->size = 1 + 2*one_kb;
    MetaData* m6 = new MetaData;
    m6->size = 1+3*one_kb;
    insertToHist(m1);
    insertToHist(m2);
    insertToHist(m3);
    insertToHist(m4);
    insertToHist(m5);
    insertToHist(m6);
    print_hist();
    MetaData* m7 = findFreeSpace(5);
    cout << m7->size << endl;
    print_hist();
    MetaData* m8 = findFreeSpace(2100);
    cout << m8->size << endl;
    print_hist();
    exit(0);
}

static void insertToAllocatedList(MetaData* to_insert){
    MetaData* it = heapHead;

    if(!it){
        to_insert->next = NULL;
        to_insert->prev = NULL;
        heapHead = to_insert;
        heapTail = to_insert;
        return;
    }
    if(to_insert < it){
        to_insert->next = heapHead;
        heapHead->prev = to_insert;
        heapHead = to_insert;
        return;
    }
    while(it < to_insert){
        it = it->next;
    }
    if(!it){
        heapTail->next = to_insert;
        to_insert->prev = heapTail;
        heapTail = to_insert;
        return;
    }
    to_insert->prev = it->prev;
    to_insert->next = it;
    it->prev = to_insert;
    to_insert->prev->next = to_insert;
}



void* smalloc(size_t size){
    if(size > MAX_SIZE || size == 0){
        return NULL;
    }
    MetaData* out = findFreeSpace(size);

    if(!out) {
        out = checkWilderness();
        if(!out) {
            out = (MetaData *) (sbrk(intptr_t(sizeof(MetaData) + size)));
            if (out == (void *) (-1)) {
                exit(1);
            }
            insertToAllocatedList(out);
        }else{
            out = (MetaData*) sbrk(size - out->size);
            if(out == (void*)(-1)){
                return NULL;
            }
            out->size = size;
            insertToAllocatedList(out);
        }

    }else{
        insertToAllocatedList(out);
    }
    return (void*) (out + sizeof(MetaData));
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