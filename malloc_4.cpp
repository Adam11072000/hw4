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
#include <sys/mman.h>
using std::cout;
using std::endl;

const long int MAX_SIZE = pow(10, 8);

struct MetaData{
    size_t size;
    MetaData* prev;
    MetaData* next;
};
const int to_round = 4;

size_t alignSize(size_t size){
    if(size % to_round == 0){
        return size;
    }
    return size + (to_round - size % to_round);
}


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
static void mergeAdjacent(MetaData* target);
static MetaData* checkWilderness();
static void insertToAllocatedList(MetaData* to_insert);
static void insertToMapped(MetaData* to_insert);


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
            tmp = it;
            tmp += 1;
            incVoidPtr((void*)tmp, it->size);
            if(tmp == (void*)sbrk(0)){
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
        MetaData* secondHalf = *to_split;
        secondHalf += alignSize(size);
        incVoidPtr((void*)secondHalf, size);
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
        return;
    }
    if(it > metadata){
        metadata->next = it;
        it->prev = metadata;
        metadata->prev = NULL;
        free_hist[metadata->size / one_kb] = metadata;
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
    MetaData* tmp;

    for(int i = 0; i < hist_size; i++){
        it = free_hist[i];
        while(it){
            tmp = target_block;
            tmp = (MetaData*)incVoidPtr(tmp, (long int)(-it->size) - alignSize(sizeof(MetaData)));
            if(tmp == it){
                to_return_lower = it;
            }
            tmp = target_block + alignSize(sizeof(MetaData));
            tmp = (MetaData*) incVoidPtr(tmp, it->size);
            if(tmp == it) {
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

static void mergeLower(MetaData* target, MetaData* lower){
    removeFreeBlock(lower);
    removeFreeBlock(target);
    lower->size = lower->size + target->size + alignSize(sizeof(MetaData));
    insertToHist(lower);
}

static void mergeHigher(MetaData* target, MetaData* upper){
    removeFreeBlock(target);
    removeFreeBlock(upper);
    target->size = target->size + upper->size + alignSize(sizeof(MetaData));
    insertToHist(target);
}

static void mergeBoth(MetaData* target, MetaData* lower, MetaData* upper){
    removeFreeBlock(lower);
    removeFreeBlock(target);
    removeFreeBlock(upper);
    lower->size = lower->size + target->size + upper->size + alignSize(2*sizeof(MetaData));
    insertToHist(lower);
}

static void mergeAdjacent(MetaData* target){
    MetaData* upper = NULL;
    MetaData* lower = findAdjacentBlocks(target, &upper);

    if(!lower && !upper){
        return;
    } else if(lower && !upper){
        mergeLower(target, lower);
    } else if(!lower && upper){
        mergeHigher(target, upper);
    } else{
        mergeBoth(target, lower, upper);
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
            it = it->next;
        }
    }
    std::cout << "======================================================================================" << endl;
}

//int main(){
//    MetaData* m1 = new MetaData;
//    m1->size = 1;
//    MetaData* m2 = new MetaData;
//    m2->size = 2;
//    MetaData* m3 = new MetaData;
//    m3->size = 3;
//    MetaData* m4 = new MetaData;
//    m4->size = 1+one_kb;
//    MetaData* m5 = new MetaData;
//    m5->size = 1 + 2*one_kb;
//    MetaData* m6 = new MetaData;
//    m6->size = 1+3*one_kb;
//    insertToHist(m1);
//    insertToHist(m2);
//    insertToHist(m3);
//    insertToHist(m4);
//    insertToHist(m5);
//    insertToHist(m6);
//    print_hist();
//    MetaData* m7 = findFreeSpace(5);
//    cout << m7->size << endl;
//    print_hist();
//    MetaData* m8 = findFreeSpace(2100);
//    cout << m8->size << endl;
//    print_hist();
//    exit(0);
//}

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
    while(it && it < to_insert){
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
        out = (MetaData*) mmap(NULL, alignSize(sizeof(MetaData)) + alignSize(size), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if(out == (void*)(-1)){
            return NULL;
        }
        out->size = alignSize(size);
        insertToMapped(out);
        return incVoidPtr(out, alignSize(sizeof(MetaData)));
    }

    out = findFreeSpace(size);

    if(!out) {
        out = checkWilderness();
        if(!out) {
            out = (MetaData *) (sbrk(alignSize(sizeof(MetaData)) + alignSize(size)));
            if (out == (void *) (-1)) {
                return NULL;
            }
            insertToAllocatedList(out);
        }else{
            out = (MetaData*) sbrk(alignSize(size) - out->size);
            if(out == (void*)(-1)){
                return NULL;
            }
            insertToAllocatedList(out);
        }
        out->size = alignSize(size);
    }else{
        insertToAllocatedList(out);
    }
    out = (MetaData*)incVoidPtr(out, alignSize(size));
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
    std::memset(out,0, alignSize(size*num)); // check ascii
    return out;
}


MetaData* removeFromMapped(void* target){
    MetaData* it = mappedHead;
    while(it){
        if(incVoidPtr(it, alignSize(sizeof(MetaData))) == target){
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
MetaData* removeFromAllocatedList(void* target){
    MetaData* it = heapHead;
    while(it){
        if(incVoidPtr(it, alignSize(sizeof(MetaData))) == target){
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

void sfree(void* p) {
    if (p == NULL) {
        return;
    }
    if(((MetaData*)incVoidPtr(p, -alignSize(sizeof(MetaData))))->size < threshold) {
        MetaData *to_insert = removeFromAllocatedList(p);
        if (to_insert) {
            insertToHist(to_insert);
            mergeAdjacent(to_insert);
        }
    }else{
        removeFromMapped(p);
        munmap(((MetaData*)incVoidPtr(p, -alignSize(sizeof(MetaData)))),
               ((MetaData*)incVoidPtr(p, -alignSize(sizeof(MetaData))))->size);
    }
}

void* srealloc(void* oldp, size_t size){
    if(size == 0 or size > MAX_SIZE){
        return NULL;
    }
    void* out;
    if(size >= threshold){
        MetaData* it = ((MetaData*)incVoidPtr(oldp, -alignSize(sizeof(MetaData))));
        if(it->size >= alignSize(size)){
            return oldp;
        }
        removeFromMapped(oldp);
        out = smalloc(size);
        size_t to_copy = size > ((MetaData*)incVoidPtr(oldp, -alignSize(sizeof(MetaData))))->size ?
                ((MetaData*)incVoidPtr(oldp, -alignSize(sizeof(MetaData))))->size : alignSize(size);
        memcpy(out, oldp, to_copy);
        return out;
    }


    bool was_alloc = false;
    size_t to_copy;
    if(oldp){
        size_t to_copy = size > ((MetaData*)incVoidPtr(oldp, -alignSize(sizeof(MetaData))))->size ?
                        ((MetaData*)incVoidPtr(oldp, -alignSize(sizeof(MetaData))))->size : alignSize(size);
        was_alloc = true;
        MetaData* it = heapHead;
        // try to use curr block
        while(it){
            if(incVoidPtr(oldp, alignSize(sizeof(MetaData))) == oldp){
                if(it->size >= alignSize(size)){
                    split(&it, alignSize(size));
                    return oldp;
                }
                break;
            }
            it = it->next;
        }
        removeFromAllocatedList(incVoidPtr(oldp, -alignSize(sizeof(MetaData))));
        out = mergeAdjacentByPriority((MetaData*)incVoidPtr(oldp, -alignSize(sizeof(MetaData))), size);
        if(out){
            removeFreeBlock((MetaData*) out);
            memcpy(incVoidPtr(oldp, alignSize(sizeof(MetaData))), oldp, to_copy);
            insertToAllocatedList((MetaData*)out);
            return incVoidPtr(out, alignSize(sizeof(MetaData)));
        }
    }

    out = smalloc(size);
    if(out == NULL){
        return NULL;
    }
    if(was_alloc) {
        memcpy(out, oldp, to_copy);
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
            it = it->next;
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
            it = it->next;
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
    return res + _num_free_blocks();
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
    res += _num_free_bytes();
    return res;
}

size_t _num_meta_data_bytes(){
    return sizeof(MetaData)*_num_allocated_blocks();
}

size_t _size_meta_data(){
    return sizeof(MetaData);
}