#ifndef AMBER_INCLUDE_RDMA_RDMA_H
#define AMBER_INCLUDE_RDMA_RDMA_H

#include <rdma/rdma_verbs.h>
#include <arpa/inet.h>
#include <cerrno>
#include <system_error>
#include <cstring>
#include <thread>
#include <queue>
#include <atomic>
#include <cassert>
#include <mutex>
#include <functional>
#include <malloc.h>

#include "rdma_message.h"
#include "buf_list.h"
#include "safe_queue.h"

namespace amber {
namespace rdma {

class RDMA_Device
{
private:
    struct RDMA_Remote_Info
    {
        //from which curr cm_id index
        int idx;
        //recv buffer
        char* buf;
        char* last_buf;
        //mr
        ibv_mr* mr;
        //remote cm_id
        rdma_cm_id* remote_cm_id;
        std::shared_ptr<RDMA_Message> ptr;
    };

protected:
    std::atomic<int> m_sz;
    std::thread* m_thd;
    std::atomic<bool> m_Runing;
    RDMA_Remote_Info* m_remote_info;

protected:
    RDMA_Device(int device_num);
    ~RDMA_Device();
    rdma_cm_id* ResolveAddress(char* port, char* host);
    bool PollCQ(ibv_cq* cq);
    void RegistBuffer(int idx, void* buffer, int32_t length);
    int SetQPTimeout(ibv_qp* qp);
};

class RDMA_Client : public RDMA_Device
{
private:
    SafeQueue<std::shared_ptr<RDMA_Message>>* m_queue;
    bool* m_sending;
    std::mutex* m_mutex;

private:
    void Sending(); 

public:
    RDMA_Client(int max_svr_num);
    ~RDMA_Client();
    int Connect(const std::string& host, const std::string& port, void* buffer, size_t length);
    void Send(std::shared_ptr<RDMA_Message> buffer, int idx = -1);
    void Stop();
};

class RDMA_Server : public RDMA_Device
{
private:
    using HandleMsg = std::function<void(std::shared_ptr<RDMA_Message>&)>;

private:
    struct RDMA_Local_Info
    {
        rdma_cm_id* local_cm_id;
        BufferList* bufList;
    };

private:
    int m_local_info_sz;
    RDMA_Local_Info* m_local_info;


private:
    void Recving(const HandleMsg& Handle);

public:
    RDMA_Server(int max_client_num, int max_qp_num, const HandleMsg& Handle);
    ~RDMA_Server();
    int Listen(const std::string& port, int blk_size, int queue_size = 500);
    void Accept(int idx);
    void Stop();
};

}
}

#endif
