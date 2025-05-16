#include <Arduino.h>
#include "HNBuffer.h"

HNBuffer::HNBuffer() : front(0), rear(0), count(0) {}

bool HNBuffer::isFull() {
    return count == QUEUE_SIZE;
}

bool HNBuffer::isEmpty() {
    return count == 0;
}

bool HNBuffer::enqueue(uint8_t note, uint8_t velocity) {
    if (isFull()) {
        return false; // La cola está llena
    }
    data[rear][0] = note;
    data[rear][1] = velocity;
    rear = (rear + 1) % QUEUE_SIZE;
    count++;
    return true;
}

bool HNBuffer::dequeue(uint8_t &note, uint8_t &velocity) {
    if (isEmpty()) {
        return false; // La cola está vacía
    }
    note = data[front][0];
    velocity = data[front][1];
    front = (front + 1) % QUEUE_SIZE;
    count--;
    return true;
}