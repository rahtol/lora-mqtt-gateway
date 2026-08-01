#include <cstdint>

template <typename T>
class FifoBufferClass
{
  public:
    FifoBufferClass(int capacity = 8, int buffer_size = 258) :
        head(0),
        tail(0),
        count(0),
        lost_packets(0),
        received_packets(0)
    {
        this->capacity = capacity + 1; // one slot is used to detect full/empty condition
        this->buffer_size = buffer_size;
        buffer = new T*[this->capacity];
        for (int i = 0; i < this->capacity; i++) {
            buffer[i] = new T[buffer_size];
        }
    }

    ~FifoBufferClass()
    {
        for (int i = 0; i < this->capacity; i++) {
            delete[] buffer[i];
        }
        delete[] buffer;
    }

    T* getNextHeadBufferPtr()
    {
        head = (head + 1) % capacity;
        if (head == tail) {
            // buffer overflow, move tail forward and count lost packet
            lost_packets++;
            tail = (tail + 1) % capacity;
        }
        else {
            // new packet, increase count
            count++;
            received_packets++;
        }
        return buffer[head];
    }

    T* pop(bool remove = true)
    {
        if (isEmpty()) {
            return nullptr;
        }
        T *p = buffer[tail];
        if (remove) {
            tail = (tail + 1) % capacity;
            count--;
        }
        return p;
   }

    bool isEmpty()
    {
        return head == tail;
    }

    bool isFull()
    {
        return (head + 1) % capacity == tail;
    }

    int getLostPacketCount()
    {
        return lost_packets;
    }

  private:
    int capacity;
    int buffer_size;
    T** buffer;
    int head;
    int tail;
    int count;
    int lost_packets;
    int received_packets;
};