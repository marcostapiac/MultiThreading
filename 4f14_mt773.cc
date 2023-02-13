//
//  main.cpp
//  Coursework-4F14-mt773
//
//  Created by Marcos Tapia Costa on 4/3/22.
//

#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>

std::mutex outputmutex; // For easier debugging
struct node{
    std::string String;
    std::mutex m;
    int Integer;
    bool isNull = false;
    struct node *next; // Node which arrived afterwards
    struct node *prev; // Node which arrived beforehand
};
struct queue{
    // Implements a doubly linked list queue
    // Need to know front and end of queue at all times
    struct node* back;
    struct node* front;
    std::mutex size_mutex;
    int size = 0;
    queue(){
        front = new node;
        front->isNull = true;
        back = front; // Save memory
    }
    void Add(std::string stringToAdd, int integerToAdd){
        // No need for locks since function called by main thread in questionB() before other threads begin
        struct node* newnode;
        newnode = new node; // Instantiate node
        // Next node is a Null Node
        newnode->next = new node;
        newnode->next->isNull = true; // NULL since newnode is at the end of the queue
        newnode->String = stringToAdd;
        newnode->Integer = integerToAdd;
        newnode->prev = back; // Node which arrived previously is the last element in queue
        // Update queue 'back' pointer.
        if (back->isNull){
            // Executes when there are no nodes in the queue
            back = newnode;
            // No need to lock front node since back = front initially
            front = newnode;
        } else {
            back->next = newnode; // Update forward pointer of latest node
            back = newnode; // Update the newest arrival in queue
        };
        std::lock_guard<std::mutex> sizelock(size_mutex); // Lock now
        this->size++;
    };
    void reverseAndPrint(){
        std::unique_lock<std::mutex> outputlock(outputmutex, std::defer_lock);
        std::unique_lock<std::mutex> lock_temp_node;
        struct node* temp;
        struct node* nexttemp;
        // Always hold to head--> no other thread can start if reverseAndPrint has locked onto head
        std::unique_lock<std::mutex> lock_head_node(front->m);
        // Ensure removeItem hasn't changed front pointer
        while (lock_head_node.mutex() != &(front->m)){
            lock_head_node.unlock();
            lock_head_node= std::unique_lock<std::mutex>(front->m);
        };
        // Ensure removeItem hasn't removed the last node
        if (not front->isNull){
            int sum = 0;
            sum += front->Integer;
            // No need to check if pointer is correct: if front->next being deleted, removeItem holds lock on front node.
            std::unique_lock<std::mutex> lock_nexttemp_node(front->next->m);
            nexttemp = front->next;
            // Swap pointers round
            front->next = front->prev;
            front->prev = nexttemp;
            // Check if more than one node
            if (not nexttemp->isNull){
                std::unique_lock<std::mutex> dummy_lock(nexttemp->next->m); // Now holding three locks
                swap(lock_temp_node, lock_nexttemp_node);
                swap(lock_nexttemp_node, dummy_lock); // Dummy lock holds no mutex
                temp = nexttemp;
                nexttemp = nexttemp->next;
                while (nexttemp->isNull == false){
                    sum += temp->Integer;
                    temp->next = temp->prev;
                    temp->prev = nexttemp;
                    dummy_lock = std::unique_lock<std::mutex>(nexttemp->next->m);
                    swap(lock_nexttemp_node, dummy_lock);
                    swap(lock_temp_node, dummy_lock);
                    temp = nexttemp;
                    nexttemp = nexttemp->next;
                    dummy_lock.unlock(); // Remove lock on previous temp node
                };
                sum += temp->Integer;
                temp->next = temp->prev;
                temp->prev = nexttemp;
                // Temp is the back node; swap front and back
                struct node* dummy;
                dummy = front;
                front = back;
                back = dummy;
                // Integer sum will be output AFTER reversal has happened but BEFORE head node is released,
                // so printing thread CANT start if sum isn't output
                outputlock.lock();
                std::cout << "Integer sum is: " << sum << "\n";
                outputlock.unlock();
                // Unlock in order -> first the head, then the tail
                lock_temp_node.unlock(); // New HEAD
                lock_head_node.unlock(); // New TAIL
            } else {
                // Front == back so back is locked too
                // Integer sum will be output AFTER reversal has happened but BEFORE head node is released,
                // so printing thread CANT start if sum isn't output
                outputlock.lock();
                std::cout << "Integer sum is: " << sum << "\n";
                outputlock.unlock();
                lock_head_node.unlock();
            };
        };
    };
    void print(){
        /* Iterates in a forward direction through queue: can start before removal finishes, but only AFTER reversal finishes  */
        std::unique_lock<std::mutex> outputlock(outputmutex, std::defer_lock);
        struct node* temp;
        std::unique_lock<std::mutex> lock_temp_node(front->m); // Lock now
        // Check if front has been changed by reversal or removal since acquiring lock on muxtex
        while (lock_temp_node.mutex() != &(front->m)){
            lock_temp_node.unlock();
            lock_temp_node= std::unique_lock<std::mutex>(front->m);
        };
        temp = front;
        // Forward direction of printing
        while (temp->isNull == false){
            outputlock.lock();
            std::cout << temp->String << " " << temp->Integer << "\n";
            outputlock.unlock();
            // Hand-over-hand locking
            std::unique_lock<std::mutex> dummy_lock(temp->next->m); // Lock now
            swap(lock_temp_node, dummy_lock);
            temp = temp->next;
            dummy_lock.unlock(); // Release lock on current node
        };
    };
    void removeItem(int identifier){
        /* Removes item at index "identifier" */
        struct node* temp;
        struct node* prevtemp;
        struct node* nexttemp;
        std::unique_lock<std::mutex> outputlock(outputmutex, std::defer_lock);
        std::unique_lock<std::mutex> sizelock(this->size_mutex, std::defer_lock);
        std::unique_lock<std::mutex> lock_temp_node(front->m); // Lock now
        // Check if reversal has changed front pointer obtained in previous line
        while (lock_temp_node.mutex() != &(front->m)){
            lock_temp_node.unlock(); // Front pointer has been changed by reversal, so we are currently holding back node
            lock_temp_node= std::unique_lock<std::mutex>(front->m);
        };
        std::unique_lock<std::mutex> lock_prevtemp_node(front->prev->m, std::defer_lock);
        std::unique_lock<std::mutex> lock_nexttemp_node(front->next->m, std::defer_lock);
        std::lock(lock_nexttemp_node, lock_prevtemp_node); // Avoid locking deadlock
        temp = front;
        prevtemp = front->prev;
        nexttemp = front->next;
        for (int i = 0; i < identifier; i++){
            std::unique_lock<std::mutex> dummy_lock(nexttemp->next->m);
            swap(lock_nexttemp_node, dummy_lock); // lock_nexttemp holds lock on next nexttemp
            swap(lock_temp_node, dummy_lock); // lock_temp holds lock on nexttemp
            swap(lock_prevtemp_node, dummy_lock); // lock_prevtemp holds lock on temp
            prevtemp = temp;
            temp = nexttemp;
            nexttemp = temp->next;
            dummy_lock.unlock(); // Remove lock on previous prev_temp
        };
        sizelock.lock(); // Lock size here since removal about to begin
        prevtemp->next = nexttemp;
        nexttemp->prev = prevtemp;
        // Update pointers if at edges of queue
        if (temp == front){
            // We have a lock on temp, and therefore a lock on front
            // If temp != front, then EITHER printing has front locked (don't care because printing doesn't change pointer value)
            // OR reversal has a lock on front, but it cannot finish before removal has finished, so it won't have changed its value.
            front = nexttemp;
            if ((nexttemp->isNull)){
                // Just removed last node in the queue --> front == back, so back is locked too
                back = front; // Update back pointer
            }
        } else if (temp == back){
            // We have a lock on temp, and therefore a lock on back
            // If temp != back, then either printing has it locked (don't care because it doesn't change value of it)
            // Since reversal can only be executing "behind" removal, back pointer will not have been changed by reversal yet
            back = prevtemp;
        };
        this->size--;
    };
};
void questionB(struct queue *q, const char* characters){
    srand(int(time(0))); // Ensures generated numbers are different at each run
    for (int i=0; i<80; i++){
        int randomChars = 3 + (rand() % 5); // Number of chars in string
        int randomInteger = (rand()%256); // Random integer in [0, 255]
        std::string randomString;
        for (int j=0; j <randomChars; j++){
            randomString += characters[ rand() % 26 ]; // Appends single
        };
        q->Add(randomString, randomInteger);
    };
};
void questionC(struct queue *queueToReverse){
    std::unique_lock<std::mutex> outputlock(outputmutex, std::defer_lock);
    // Continually reverses queue while not empty
    std::unique_lock<std::mutex> sizelock(queueToReverse->size_mutex); // Lock now
    while (queueToReverse->size > 0){
        sizelock.unlock(); // Minimise time lock is held
        queueToReverse->reverseAndPrint();
        sizelock.lock(); // Lock before reading size of queue
    };
};
void questionD(struct queue *queueToPrint){
    std::unique_lock<std::mutex> sizelock(queueToPrint->size_mutex); // Lock now
    while (queueToPrint->size > 0){
        sizelock.unlock(); // Minimise time lock is held
        queueToPrint->print();
        sizelock.lock(); // Lock before reading size of queue
    };
};
void questionE(struct queue *queue){
    // Since we only remove items in this thread, never read nonzero size in while condition which changes to 0 during single loop iter
    std::unique_lock<std::mutex> sizelock(queue->size_mutex); // Lock now
    while (queue->size > 0){
        // Unlock size here so other threads can START before final node removed
        sizelock.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(200)); // Wait 0.2s = 200 000 000 nanoseconds
        int position = rand()%queue->size; // Doesn't matter is queue->size isn't locked since only this thread can alter its value
        queue->removeItem(position);
        sizelock.lock(); // Lock before reading size of queue
    };
};
int main(int argc, const char * argv[]) {
    queue* q; // Pointer to new queue
    static const char chars[] = "abcdefghijklmnopqrstuvwxyz"; // English Language
    q = new queue(); // Initialise front and back pointers
    questionB(q,chars); // Add nodes to queue
    q->print();
    std::thread t1(questionC, q); // Pass reference to queue
    t1.detach(); // Make thread run in the background
    std::thread t2(questionD, q);
    t2.detach();
    std::thread t3(questionE, q);
    t3.detach();
    return 0;
}
