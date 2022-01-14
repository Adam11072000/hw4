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
const size_t one_kb = 1024; // make sure not 1000
MetaData* free_hist[hist_size] = {nullptr};

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
MetaData* findFreeSpace(size_t size){
    MetaData* to_return = NULL;
    MetaData* targeted_list;
    MetaData* it;

    for(size_t i = size; i < hist_size; i+=one_kb){
        targeted_list = free_hist[i / one_kb];
        if(targeted_list){
            it = targeted_list;
            while(it){
                if(it->size >= size){
                    to_return = it;
                    if(it == targeted_list){
                        free_hist[i / one_kb] = it->next;
                        if(it->next){
                            it->next->prev = NULL;
                        }
                        return to_return;
                    } else{
                        if(it->next != NULL){
                            it->next->prev = it->prev;
                        }
                       it->prev->next = it->next;
                       return to_return;
                    }
                }
                it = it->next;
            }
        }
    }
    return NULL;
}

using std::cout;
using std::endl;
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
}

int main(){
    MetaData* m1 = new MetaData;
    m1->size = 1;
    MetaData* m12 = new MetaData;
    m1->size = 2;
    MetaData* m13 = new MetaData;
    m1->size = 3;
    MetaData* m2 = new MetaData;
    m2->size = 1+one_kb;
    MetaData* m3 = new MetaData;
    m3->size = 1+2*one_kb;
    MetaData* m4 = new MetaData;
    m4->size = 1+3*one_kb;
    insertToHist(m1);
    insertToHist(m2);
    insertToHist(m3);
    insertToHist(m4);
    insertToHist(m12);
    insertToHist(m13);
    print_hist();
    exit(0);
}


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