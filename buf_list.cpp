#include "buf_list.h"

#include <sys/time.h>

#define CAS(x, oldval, newval) __sync_bool_compare_and_swap(&(x), oldval, newval)

namespace amber {
namespace rdma {

BufferList::BufferList(int size, int num) 
    : m_size(size), m_cap(size * num)
{
    m_head = 0;
    m_tail = num - 1;
    m_buf  = (char*)memalign(1 << 12, size * num);
    m_next = (int*)memalign(1 << 12, sizeof(int) * num);
    for (int i = 0; i < num - 1; ++i)
    {
        m_next[i] = i + 1;
    }
    m_next[num - 1] = -1;
}

BufferList::~BufferList()
{
    free(m_next);
    free(m_buf);
}

char* BufferList::GetBuffer(int idx)
{
    return m_buf + idx * m_size;
}

char* BufferList::Head()
{
    return (char*)GetBuffer(0);
}

int BufferList::Capcity()
{
    return m_cap;
}

int BufferList::Size()
{
    return m_size;
}

char* BufferList::GetFreeBuf()
{
    int head, next;
    do
    {
        head = m_head;
        if (head == -1)
        {
            return nullptr;
        }
        next = m_next[head];
    }while (!CAS(m_head, head, next));
    return GetBuffer(head);
}

void BufferList::FreeBuf(char* buf)
{
    int idx = (buf - m_buf) / m_size;

    //may be last obj free first
    if (m_tail == idx)
    {
        CAS(m_head, -1, idx);
        return ;
    }

    m_next[idx] = -1;

    int curr_tail = m_tail;
    auto tail = curr_tail;

    do
    {
        while (m_next[tail] != -1)
        {
            tail = m_next[tail];
        }
    }while(!CAS(m_next[tail], -1, idx));

    if (CAS(m_tail, curr_tail, idx))
    {
        CAS(m_head, -1, idx);
    }
}

}
}
