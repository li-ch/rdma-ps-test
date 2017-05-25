#ifndef AMBER_PS_RDMA_BUF_LIST_H
#define AMBER_PS_RDMA_BUF_LIST_H

#include <cstdio>
#include <iostream>
#include <malloc.h>
#include <cstring>
#include <mutex>

namespace amber {
namespace rdma {

class BufferList
{
private:
    const int m_size;
    const int m_cap;
    char* m_buf;
    int* m_next;
    int m_head;
    int m_tail;

private:
    char* GetBuffer(int idx);

public:
    BufferList(int size, int num);
    ~BufferList();
    char* GetFreeBuf();
    void FreeBuf(char* buf);

    char* Head();
    int Capcity();
    int Size();
};

}
}
#endif
