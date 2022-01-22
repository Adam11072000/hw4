//
// Created by student on 1/14/22.
//
#include <unistd.h>
#include <cstring>
#include <cmath>
#include <iostream>
#include <sys/mman.h>
#include <cassert>

using std::cout;
using std::endl;

const long int MAX_SIZE = pow(10, 8);

struct MetaData{
    size_t size;
    MetaData* prev;
    MetaData* next;
    bool is_free;
    MetaData* next_free;
    MetaData* prev_free;
};



MetaData* heapHead = NULL;
MetaData* heapTail = NULL; // last element
MetaData* mappedHead = NULL;
MetaData* mappedTail = NULL; // last element
const int hist_size = 128;
const int min_split = 128;
const size_t one_kb = 1024; // make sure not 1000
const size_t threshold = one_kb*hist_size;
MetaData* free_hist[hist_size] = {nullptr};


static MetaData* removeFreeBlock(MetaData* target);
static void insertToHist(MetaData* metadata);
static MetaData* findAdjacentBlocks(MetaData* target_block, MetaData** to_return_upper);
static MetaData* mergeAdjacent(MetaData* target);
static MetaData* checkWilderness();
static void insertToAllocatedList(MetaData* to_insert);
static void insertToMapped(MetaData* to_insert);
static void print_memory_list();

MetaData* removeFromAllocatedList(void* target){
    MetaData* it = heapHead;
    while(it){
        if(it == (MetaData*)target){
            break;
        }
        it = it->next;
    }
    if(!it){
        return NULL;
    }
    if(it == heapHead){
        if(heapHead == heapTail){
            heapHead = NULL;
            heapTail = NULL;
        }else{
            heapHead = it->next;
            it->next->prev = NULL;
        }
        return it;
    }
    if(it == heapTail){
        heapTail = it->prev;
        heapTail->next = NULL;
        return it;
    }
    if(it->prev){
        it->prev->next = it->next;
    }
    if(it->next){
        it->next->prev = it->prev;
    }
    return it;
}

static void* incVoidPtr(void* ptr, long int bytes){
    long int to_move = bytes > 0 ? 1 : -1;

    while(bytes != 0){
        ptr = (char*)ptr + to_move;
        bytes -= to_move;
    }

    return ptr;
}


static MetaData* checkWilderness() {
    MetaData* it;
    MetaData* tmp;

    for(int i = 0; i < hist_size; i++){
        it = free_hist[i];
        while(it){
            tmp = (MetaData*)incVoidPtr((void*)it, it->size + sizeof(MetaData));
            if(tmp == sbrk(0)){
                removeFreeBlock(it);
                return it;
            }
            it = it->next_free;
        }
    }
    return NULL;
}

static MetaData* split(MetaData* to_split, size_t size) {

    if (to_split->size - size - sizeof(MetaData) < min_split) {
        return to_split;
    }

    if (to_split->size > size) {
        removeFreeBlock(to_split);
        int old_size = to_split->size;
        MetaData *secondHalf = (MetaData *) incVoidPtr((void *)to_split, size + sizeof(MetaData));
        secondHalf->size = old_size - size - sizeof(MetaData);
        to_split->is_free = false;
        to_split->size = size;
        secondHalf->is_free = true;
        insertToAllocatedList(secondHalf);
        insertToHist(secondHalf);
        mergeAdjacent(secondHalf);
    }
    return to_split;
}

/** size is def smaller than 128kb */
static void insertToHist(MetaData* metadata){
    MetaData* it = free_hist[metadata->size / one_kb];
    MetaData* prev = free_hist[metadata->size / one_kb];

    if(!it){
        free_hist[metadata->size / one_kb] = metadata;
        metadata->next_free = NULL;
        metadata->prev_free = NULL;
        return;
    }
    if(it > metadata){
        metadata->next_free = it;
        it->prev_free = metadata;
        metadata->prev_free = NULL;
        free_hist[metadata->size / one_kb] = metadata;
        return;
    }
    while(it && it < metadata){
        prev = it;
        it = it->next_free;
    }
    if(it != NULL){ // if here then it >= metadata
        it->prev_free = metadata;
    }
    metadata->prev_free = prev;
    prev->next_free = metadata;
    metadata->next_free = it;
}

/** get the first empty space, return null if none is found */
static MetaData* findFreeSpace(size_t size){
    MetaData* it = NULL;
    size_t index = -1;
    MetaData* min = NULL;
    bool flag = false;
    for(size_t i = size; i < hist_size*one_kb; i+=one_kb){
        if(flag){
            break;
        }
        it = free_hist[i / one_kb];
        while(it){
            if(it->size >= size && (min == NULL || it < min)){
                min = it;
                index = i;
                flag = true;
                break; // we won't find a lower address block
            }
            it = it->next_free;
        }
    }
    if(min == NULL){
        return NULL;
    }
    if(min == free_hist[index / one_kb]){
        free_hist[index / one_kb] = min->next_free;
        if(min->next_free){
            min->next_free->prev_free = NULL;
        }
    }else{
        if(min->next_free != NULL){
            min->next_free->prev_free = min->prev_free;
        }
        min->prev_free->next_free = min->next_free;
    }
    if(min->size > size){
        min = split(min, size);
    }
    return min;
}

static MetaData* findAdjacentBlocks(MetaData* target_block, MetaData** to_return_upper){
    MetaData* it;
    MetaData* to_return_lower = NULL;
    *to_return_upper = NULL;

    if(target_block->prev && target_block->prev->is_free){
        to_return_lower = target_block->prev;
    }
    if(target_block->next && target_block->next->is_free){
        *to_return_upper = target_block->next;
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
            free_hist[i] = it->next_free;
            if(it->next_free){
                it->next_free->prev_free = NULL;
            }
            return it;
        }
        // def not list head
        if(it) {
            it = it->next_free;
        }
        while(it){
            if(it == target){
                it->prev_free->next_free = it->next_free;
                if(it->next_free){
                    it->next_free->prev_free = it->prev_free;
                }
                return it;
            }
            it = it->next_free;
        }
    }
    return NULL;
}

static void mergeLower(MetaData* target, MetaData* lower){
    removeFreeBlock(lower);
    removeFreeBlock(target);
    removeFromAllocatedList(target);
    lower->size = lower->size + target->size + sizeof(MetaData);
    insertToHist(lower);
}

static void mergeHigher(MetaData* target, MetaData* upper){
    removeFreeBlock(target);
    removeFreeBlock(upper);
    removeFromAllocatedList(upper);
    target->size = target->size + upper->size + sizeof(MetaData);
    insertToHist(target);
}

static void mergeBoth(MetaData* target, MetaData* lower, MetaData* upper){
    removeFreeBlock(lower);
    removeFreeBlock(target);
    removeFreeBlock(upper);
    removeFromAllocatedList(target);
    removeFromAllocatedList(upper);
    lower->size = lower->size + target->size + upper->size + 2*sizeof(MetaData);
    insertToHist(lower);
}

static MetaData* mergeAdjacent(MetaData* target){
    MetaData* upper = NULL;
    MetaData* lower = findAdjacentBlocks(target, &upper);

    if(!lower && !upper){
        return target;
    } else if(lower && !upper){
        mergeLower(target, lower);
        return lower;
    } else if(!lower && upper){
        mergeHigher(target, upper);
        return target;
    } else{
        mergeBoth(target, lower, upper);
        return lower;
    }
}

static MetaData* mergeAdjacentByPriority(MetaData* target, size_t new_size){
    MetaData* upper = NULL;
    MetaData* lower = findAdjacentBlocks(target, &upper);
    if(lower && target->size + lower->size >= new_size){
        mergeLower(target, lower);
        return lower;
    }
    if(upper && target->size + upper->size >= new_size){
        mergeHigher(target, upper);
        return target;
    }
    if(lower && upper && target->size + upper->size + lower->size >= new_size){
        mergeBoth(target,lower,upper);
        return lower;
    }
    return NULL;
}


void print_hist(){
    for(int i = 0; i < hist_size; i++){
        MetaData* it = free_hist[i];
        if(it){
            std::cout << "this is free_hist[" << i << "]:===================" << std::endl;
        }
        while(it){
            std::cout << it->size << endl;
            it = it->next_free;
        }
    }
    std::cout << "======================================================================================" << endl;
}

static void insertToAllocatedList(MetaData* to_insert){
    MetaData* it = heapHead;
    to_insert->prev_free = NULL;
    to_insert->next_free = NULL;
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
    while(it && it < to_insert){
        it = it->next;
    }
    if(!it){
        heapTail->next = to_insert;
        to_insert->prev = heapTail;
        heapTail = to_insert;
        return;
    }
    to_insert->next = it;
    to_insert->prev = it->prev;
    if(it->prev){
        it->prev->next = to_insert;
    }
    it->prev = to_insert;
}

static void insertToMapped(MetaData* to_insert){
    MetaData* it = mappedHead;

    if(!it){
        to_insert->next = NULL;
        to_insert->prev = NULL;
        mappedHead = to_insert;
        mappedTail= to_insert;
        return;
    }
    if(to_insert < it){
        to_insert->next = mappedHead;
        mappedHead->prev = to_insert;
        mappedHead = to_insert;
        return;
    }
    while(it && it < to_insert){
        it = it->next;
    }
    if(!it){
        mappedTail->next = to_insert;
        to_insert->prev = mappedTail;
        mappedTail = to_insert;
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
    MetaData* out = NULL;
    if(size >= threshold){
        out = (MetaData*) mmap(NULL, sizeof(MetaData)+size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if(out == (void*)(-1)){
            return NULL;
        }
        out->size = size;
        out->is_free = false;
        insertToMapped(out);
        return out + 1;
    }


    out = findFreeSpace(size);

    if(!out) {
        out = checkWilderness();
        if(!out) {
            out = (MetaData *) (sbrk(intptr_t(sizeof(MetaData) + size)));
            if (out == (void *) (-1)) {
                return NULL;
            }
            insertToAllocatedList(out);
        }else{
            void* tmp = sbrk(size - out->size);
            if(tmp == (void*)(-1)){
                return NULL;
            }
        }
        out->size = size;
    }
    out->is_free = false;
    out = out + 1;
    return (void*)out;
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


MetaData* removeFromMapped(void* target){
    MetaData* it = mappedHead;
    while(it){
        if(it + 1 == target){
            break;
        }
        it = it->next;
    }
    if(!it){
        return NULL;
    }
    if(it == mappedHead){
        if(mappedHead == mappedTail){
            mappedHead = NULL;
            mappedTail = NULL;
        }else{
            mappedHead = it->next;
            it->next->prev = NULL;
        }
        return it;
    }
    if(it == mappedTail){
        mappedTail = it->prev;
        mappedTail->next = NULL;
        return it;
    }
    if(it->prev){
        it->prev->next = it->next;
    }
    if(it->next){
        it->next->prev = it->prev;
    }
    return it;
}

/**target is a user pointer*/
MetaData* getFromAllocatedList(void* target){
    MetaData* it = heapHead;
    while(it) {
        if (it + 1 == target) {
            break;
        }
        it = it->next;
    }
    return it;
}

void sfree(void* p) {
    if (p == NULL) {
        return;
    }
    if(((MetaData*)p-1)->size < threshold) {
        MetaData *to_insert = getFromAllocatedList(p);
        if (to_insert) {
            to_insert->is_free = true;
            insertToHist(to_insert);
            mergeAdjacent(to_insert);
        }
    }else{
        removeFromMapped(p);
        munmap(((MetaData*)p-1), ((MetaData*)p-1)->size + sizeof(MetaData));
    }
}

void* srealloc(void* oldp, size_t size){
    if(size == 0 or size > MAX_SIZE){
        return NULL;
    }
    void* out;
    if(size >= threshold){
        MetaData* it = (MetaData*)(oldp) - 1;
        if(it->size >= size){
            it->size = size;
            return oldp;
        }
        removeFromMapped(oldp);
        out = smalloc(size);
        size_t to_copy = size > ((MetaData*)oldp-1)->size ? ((MetaData*)oldp-1)->size : size;
        memcpy(out, oldp, to_copy);
        munmap(((MetaData*)oldp-1), ((MetaData*)oldp-1)->size + sizeof(MetaData));
        return out;
    }


    bool was_alloc = false;
    size_t to_copy;
    if(oldp){
        to_copy = size > ((MetaData*)oldp-1)->size ? ((MetaData*)oldp-1)->size : size;
        was_alloc = true;
        MetaData* curr = (MetaData*)incVoidPtr(oldp, -sizeof(MetaData));
        if(curr->size >= size){
            curr = split(curr, size);
            return curr + 1;
        }

        out = mergeAdjacentByPriority(((MetaData*)oldp-1), size);
        if(out){
            if(((MetaData*)out)->size >= size){
                out = split((MetaData*)out, size);
            }
            removeFreeBlock((MetaData*) out);
            memcpy((MetaData*)out+1, oldp, to_copy);
            ((MetaData*)out)->is_free = false;
            return incVoidPtr(out, sizeof(MetaData));
        }

        curr = mergeAdjacent(curr);
        if(curr == heapTail){
            void* tmp = sbrk(size - ((MetaData*)curr)->size);
            if(tmp == (void*)(-1)){
                return NULL;
            }
            ((MetaData*)curr)->size = size;
            ((MetaData*)curr)->is_free = false;
            removeFreeBlock(curr);
            memcpy((MetaData*)curr+1, oldp, to_copy);
            return incVoidPtr(curr, sizeof(MetaData));
        }
    }
    if(oldp){
        ((MetaData*)oldp-1)->is_free = true;
    }
    out = smalloc(size);
    if(out == NULL){
        return NULL;
    }
    if(was_alloc) {
        memcpy(out, oldp, to_copy);
        sfree(oldp);
    }
    return out;
}

size_t _num_free_blocks(){
    MetaData* it;
    size_t res = 0;

    for(int i = 0; i < hist_size; i++){
        it = free_hist[i];
        while(it){
            res++;
            it = it->next_free;
        }
    }
    return res;
}

size_t _num_free_bytes(){
    MetaData* it;
    size_t res = 0;

    for(int i = 0; i < hist_size; i++){
        it = free_hist[i];
        while(it){
            res += it->size;
            it = it->next_free;
        }
    }
    return res;
}

size_t _num_allocated_blocks(){
    MetaData* it = heapHead;
    size_t res = 0;
    while(it){
        res++;
        it = it->next;
    }
    it = mappedHead;
    while(it){
        res++;
        it = it->next;
    }
    return res;
}

size_t _num_allocated_bytes(){
    MetaData* it = heapHead;
    size_t res = 0;
    while(it){
        res += it->size;
        it = it->next;
    }
    it = mappedHead;
    while(it){
        res += it->size;
        it = it->next;
    }
    return res;
}

size_t _num_meta_data_bytes(){
    return sizeof(MetaData)*_num_allocated_blocks();
}

size_t _size_meta_data(){
    return sizeof(MetaData);
}

void print_memory_list(){
    MetaData* it = heapHead;
    while(it){
        cout << "address: " << it <<endl;
        cout << "size: " << it->size << endl;
        it = it->next;
    }
}
