//
// Created by student on 1/13/22.
//
#include <iostream>
#include <unistd.h>
#include <cmath>

const long int MAX_SIZE = pow(10, 8);

void* smalloc(size_t size){
    void* out = NULL;

    if(size == 0 || size >= MAX_SIZE){
        return NULL;
    }
    out = sbrk((intptr_t) size);
    if(out == (void*)(-1)){
        return NULL;
    }
    return out;
}
