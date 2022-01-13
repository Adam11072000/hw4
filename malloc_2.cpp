//
// Created by student on 1/13/22.
//
#include <cassert>
#include <unistd.h>

typedef enum ListResult_t{
    LIST_FAIL,
    LIST_SUCCESS
} ListResult;


class MetaData{
    int size;
    bool is_free;
    MetaData* prev;
    MetaData* next;
    void* ptr; // start of block to user
public:
    MetaData(int size, void* ptr, MetaData* prev = nullptr, MetaData* next = nullptr): size(size), ptr(ptr), prev(prev), next(next) {}
    MetaData(const MetaData& src): size(src.size), is_free(src.is_free), prev(src.prev), next(src.next){}
    ~MetaData() = default; // we may need to change
    int getSize() const;
    void setSize(int size);
    void setNext(MetaData* next);
    MetaData* getNext() const;
    MetaData* getPrevious() const;
    void setPrevious(MetaData* prev);
    void setPtr(void* ptr);
    void* getPtr() const;
};

MetaData* MetaData::getNext() const{
    return next;
}

void MetaData::setNext(MetaData* nextNode){
    this->next = nextNode;
}

MetaData* MetaData::getPrevious() const{
    return prev;
}

void MetaData::setPrevious(MetaData* previous){
    this->prev = previous;
}

void* MetaData::getPtr() const{
    return ptr;
}

void MetaData::setPtr(void* ptr){
    this->ptr = ptr;
}

int MetaData::getSize() const{
    return size;
}

void MetaData::setSize(int size) {
    this->size = size;
}

class LinkedList{
public:
    MetaData* head;
    MetaData* tail;
    int size;

public:
    LinkedList():head(nullptr), tail(nullptr),size(0){}
    LinkedList(const LinkedList& src);
    ~LinkedList();
    LinkedList& operator=(const LinkedList& src);
    MetaData* find(const MetaData& data);
    ListResult insert(const MetaData& data);
    ListResult insertWithSort(const MetaData& data);
    int getSize();
    ListResult remove(const MetaData& data);
    void printList();
    MetaData* getHead() const;
    void deleteList();
    ListResult deleteByPointer(MetaData* toDelete);
};

ListResult LinkedList::deleteByPointer(MetaData* toDelete){
    assert(toDelete);
    if(head == nullptr && tail == nullptr){
        return LIST_FAIL;
    }
    if(toDelete == head){
        head = toDelete->getNext();
    }
    if(toDelete == tail){
        tail = toDelete->getPrevious();
    }
    if(toDelete->getPrevious()){
        toDelete->getPrevious()->setNext(toDelete->getNext());
    }
    if(toDelete->getNext()){
        toDelete->getNext()->setPrevious(toDelete->getPrevious());
    }
    delete toDelete;
    size--;
    return LIST_SUCCESS;
}

void LinkedList::deleteList(){
    MetaData* tmp = head;
    while(tmp){
        tmp = head->getNext();
        delete head;
        head = tmp;
    }
    head = tail = nullptr;
}


LinkedList& LinkedList::operator=(const LinkedList& src){
    if(this == &src){
        return *this;
    }
    if(src.head == nullptr && src.tail == nullptr){
        head = tail = nullptr;
        size = 0;
        return *this;
    }
    this->deleteList();
    head = new MetaData(src.head->getSize(),src.head->getPtr());
    MetaData* iterSrc = src.head;
    MetaData* iterDest = head;
    MetaData* prev = nullptr;
    while(iterSrc){
        if(iterSrc->getNext()){
            iterDest->setNext(new MetaData(iterSrc->getSize(), iterSrc->getNext()->getPtr()));
        }
        else{
            iterDest->setNext(nullptr);
            tail = iterDest;
        }
        iterDest->setPrevious(prev);
        prev = iterDest;
        iterDest = iterDest->getNext();
        iterSrc = iterSrc->getNext();
    }
    return *this;
}

MetaData* LinkedList::getHead() const{
    return head;
}

LinkedList::LinkedList(const LinkedList& src){
    size=src.size;
    if(src.head != nullptr && src.tail != nullptr) {
        head = new MetaData(src.head->getSize(),src.head->getPtr());
        MetaData *iterSrc = src.head;
        MetaData *iterDest = head;
        while (iterSrc) {
            if (iterSrc->getNext()) {
                iterDest->setNext(new MetaData(iterSrc->getSize(), iterSrc->getNext()->getPtr()));
                iterDest->getNext()->setPrevious(iterDest);
            } else {
                iterDest->setNext(nullptr);
                tail = iterDest;
            }
            iterDest = iterDest->getNext();
            iterSrc = iterSrc->getNext();
        }
    }
    else{
        head = nullptr;
        tail = nullptr;
    }
}


LinkedList::~LinkedList(){
    this->deleteList();
}

MetaData* LinkedList::find(const MetaData& data){
    MetaData* iter = head;
    while(iter){
        if(iter->getPtr() == data.getPtr()){
            return iter;
        }
        iter = iter->getNext();
    }
    return NULL;
}

ListResult LinkedList::insert(const MetaData& new_data){
    MetaData* addedMetaData = new MetaData(new_data);
    if(head == nullptr && tail == nullptr){
        head = tail = addedMetaData;
    }
    else{
        head->setPrevious(addedMetaData);
        addedMetaData->setNext(head);
        head = addedMetaData;
    }

    size++;
    return LIST_SUCCESS;
}

ListResult LinkedList::insertWithSort(const MetaData& new_data){
    MetaData* iter = head;
    MetaData* addedMetaData = new MetaData(new_data);
    if(head == nullptr && tail == nullptr){
        head = tail = addedMetaData;
        size++;
        return LIST_SUCCESS;
    }
    while(iter && (new_data.getPtr() > iter->getPtr())){
        iter = iter->getNext();
    }
    if(iter){
        if(iter->getPrevious()){
            iter->getPrevious()->setNext(addedMetaData);
        }else{
            head = addedMetaData;
        }
        addedMetaData->setPrevious(iter->getPrevious());
        addedMetaData->setNext(iter);
        iter->setPrevious(addedMetaData);
    }else{
        tail->setNext(addedMetaData);
        addedMetaData->setNext(nullptr);
        addedMetaData->setPrevious(tail);
        tail = addedMetaData;
    }
    size++;
    return LIST_SUCCESS;
}

ListResult LinkedList::remove(const MetaData& data){
    MetaData* iter = head;
    while(iter){
        if(data.getPtr() == iter->getPtr()){
            if(iter == head){
                head = head->getNext();
            }

            if(iter == tail){
                tail = tail->getPrevious();
            }

            if(iter->getPrevious()){
                (iter->getPrevious())->setNext(iter->getNext());
            }

            if(iter->getNext()){
                (iter->getNext())->setPrevious(iter->getNext());
            }
            delete iter;
            break;
        }
        iter = iter->getNext();
    }
    if(iter == NULL){
        return LIST_FAIL;
    }
    size--;
    return LIST_SUCCESS;
}

int LinkedList::getSize(){
    return size;
}
/*
void LinkedList::printList() {
    MetaData* tmp = head;
    while(tmp){
        std::cout << tmp->getData() <<", ";
        tmp = tmp->getNext();
    }
}
*/