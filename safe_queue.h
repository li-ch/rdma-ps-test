#ifndef RDMA_SAFE_QUEUE
#define RDMA_SAFE_QUEUE

#include <cstdio>
#include <iostream>

namespace amber {
namespace rdma {

template<class T>
class SafeQueue 
{
public:
    SafeQueue()
    {
	int size = 500;
        m_buf = new T[size + 1];
        m_head = m_tail = 0;
        m_cap = size + 1;
	printf("%d %d %p", size, m_cap, m_buf);
    }

    bool Push(T& obj)
    {
        int tail = (m_tail + 1) % m_cap;
        if (tail == m_head)
            return false;
        m_buf[m_tail] = std::move(obj);
        m_tail = tail;
	return true;
    }

    void Pop()
    {
        int head = (m_head + 1) % m_cap;
        m_head = head;
    }

    T& Front()
    {
        return m_buf[m_head];
    }

    bool Empty()
    {
        return m_head == m_tail;
    }

private:
    int m_head, m_tail, m_cap;
    T* m_buf; 
};

}
}

#endif
