#include "rdma.h"

#include <sched.h>  
#include <pthread.h>
#include <sys/time.h>
#include <chrono>
#include <stdlib.h>

namespace amber {
namespace rdma {

RDMA_Device::RDMA_Device(int device_num)
{
    m_Runing = true;
    m_sz = 0;
    m_remote_info = new RDMA_Remote_Info[device_num]();
}

RDMA_Device::~RDMA_Device()
{
    for (int i = 0; i < m_sz; ++i)
    {
        rdma_destroy_ep(m_remote_info[i].remote_cm_id);
    }
    delete [] m_remote_info;
}

rdma_cm_id* RDMA_Device::ResolveAddress(char* port, char* host)
{
    rdma_addrinfo* addrinfo;
    rdma_addrinfo hints;
    memset(&hints, 0, sizeof(rdma_addrinfo));
    hints.ai_port_space = RDMA_PS_IB;

    if (host == nullptr)
    {
        hints.ai_flags = RAI_PASSIVE;
    }

    if (rdma_getaddrinfo(host, port, &hints, &addrinfo))
    {
        throw std::system_error(errno, std::system_category());
    }
    
    ibv_qp_init_attr init_attr;
    memset(&init_attr, 0, sizeof(init_attr));
    init_attr.qp_type = IBV_QPT_RC,
    init_attr.sq_sig_all = 1,
    init_attr.cap.max_send_wr = 1,
    init_attr.cap.max_recv_wr = 1,
    init_attr.cap.max_send_sge = 1,
    init_attr.cap.max_recv_sge = 1,
    init_attr.cap.max_inline_data = 0;
        
    rdma_cm_id* cm_id;
    if (rdma_create_ep(&cm_id, addrinfo, nullptr, &init_attr)) 
    {
        throw std::system_error(errno, std::system_category());
    }
    return cm_id;
}

bool RDMA_Device::PollCQ(ibv_cq* cq)
{
    ibv_wc wc[32];
    int ret = ibv_poll_cq(cq, 32, wc);
    if (ret < 0)
    {
        throw std::system_error(errno, std::system_category());
    }
    bool wc_success = true;
    for (int w=0; w<ret; ++w){
		wc_success = wc_success && (wc[w].status == IBV_WC_SUCCESS);
    }	

    return ret > 0 && wc_success;
}

void RDMA_Device::RegistBuffer(int idx, void* buffer, int32_t length)
{
    m_remote_info[idx].mr = rdma_reg_msgs(m_remote_info[idx].remote_cm_id, buffer, length);
    if (m_remote_info[idx].mr == nullptr)
    {
        throw std::system_error(errno, std::system_category());
    }
}

int RDMA_Device::SetQPTimeout(ibv_qp* qp) {
    ibv_qp_init_attr init_attr = {};
    ibv_qp_attr attr = {};

    int init_mask = IBV_QP_PKEY_INDEX |
                    IBV_QP_PORT |
                    IBV_QP_ACCESS_FLAGS;
    int rtr_mask = IBV_QP_AV |
                    IBV_QP_PATH_MTU |
                    IBV_QP_DEST_QPN |
                    IBV_QP_RQ_PSN |
                    IBV_QP_MAX_DEST_RD_ATOMIC |
                    IBV_QP_MIN_RNR_TIMER;
    int rts_mask = IBV_QP_SQ_PSN |
                    IBV_QP_TIMEOUT |
                    IBV_QP_RETRY_CNT |
                    IBV_QP_RNR_RETRY |
                    IBV_QP_MAX_QP_RD_ATOMIC;

    int mask = init_mask | rtr_mask | rts_mask;

    if (ibv_query_qp(qp, &attr, mask, &init_attr)) {
        //"Cannot query queue pair";
        return -1;
    }

    attr.qp_state = IBV_QPS_RESET;
    if (ibv_modify_qp(qp, &attr, IBV_QP_STATE)) {
        //"Cannot modify queue pair to RESET";
        return -1;
    }
    attr.qp_state = IBV_QPS_INIT;
    if (ibv_modify_qp(qp, &attr, init_mask | IBV_QP_STATE)) {
        //"Cannot modify queue pair to INIT";
        return -1;
    }
    attr.qp_state = IBV_QPS_RTR;
    attr.min_rnr_timer = 1;
    if (ibv_modify_qp(qp, &attr, rtr_mask | IBV_QP_STATE)) {
        //"Cannot modify queue pair to RTR";
        return -1;
    }
    attr.qp_state = IBV_QPS_RTS;
    attr.timeout = 1;
    if (ibv_modify_qp(qp, &attr, rts_mask | IBV_QP_STATE)) {
        //"Cannot modify queue pair timeout";
        return -1;
    }

    if (ibv_query_qp(qp, &attr, IBV_QP_TIMEOUT, &init_attr)) {
        //"Cannot query queue pair";
        return -1;
    }
    
    return 0;
}

//Client
RDMA_Client::RDMA_Client(int max_svr_num) 
    : RDMA_Device(max_svr_num)
{
    m_queue = new SafeQueue<std::shared_ptr<RDMA_Message>>[max_svr_num];
    m_thd = new std::thread(&RDMA_Client::Sending, this);
    m_sending = new bool[max_svr_num];
    m_mutex = new std::mutex[max_svr_num];
    for (int i = 0; i < max_svr_num; ++i)
        m_sending[i] = false;
}

RDMA_Client::~RDMA_Client()
{
    Stop();
    delete m_thd;
    for (int i = 0; i < m_sz; ++i)
    {
        m_sending[i] = nullptr;
    }
    delete [] m_sending;
    delete [] m_queue;
    delete [] m_mutex;
}

int RDMA_Client::Connect(const std::string& host, const std::string& port, void* buffer, size_t length)
{
    auto remote_id = ResolveAddress(const_cast<char*>(port.c_str()), const_cast<char*>(host.c_str()));
    if (rdma_connect(remote_id, nullptr)) 
    {
        rdma_destroy_ep(remote_id);
        throw std::system_error(errno, std::system_category());
    }

    if (SetQPTimeout(remoted_id->qp)) {
        rdma_destroy_ep(remote_id);
        throw std::system_error(errno, std::system_category());
    }

    m_remote_info[m_sz].remote_cm_id = remote_id;
    RegistBuffer(m_sz, buffer, length);
    return m_sz++;
    /*
    auto addr = rdma_get_local_addr(remote_id);
    printf("IP[%s], Port[%d]\n", inet_ntoa(((sockaddr_in *)addr)->sin_addr), ((sockaddr_in *)addr)->sin_port);
    */
}

void RDMA_Client::Sending()
{
    int i;
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(18, &mask);
    if (sched_setaffinity(0, sizeof(mask), &mask) == -1)
    {
        throw std::system_error(errno, std::system_category());
    }


    while (m_Runing)
    {
        for (i = 0; i < m_sz; ++i)
        {
            if (m_sending[i])
            {
                m_sending[i] = !PollCQ(m_remote_info[i].remote_cm_id->send_cq);
                if (!m_sending[i])
                {
                    m_remote_info[i].ptr = nullptr;
                }
            }
            else
            {
                //m_mutex[i].lock();
                if (!m_queue[i].Empty())
                {
                    auto buffer = std::move(m_queue[i].Front());
                    m_queue[i].Pop();
                    //m_mutex[i].unlock();
                    if (rdma_post_send(m_remote_info[i].remote_cm_id, nullptr, (void*)buffer.get(), buffer.get()->data_len + sizeof(RDMA_Message), m_remote_info[i].mr, i))
                    {
                        throw std::system_error(errno, std::system_category());
                    }
                    m_sending[i] = true;
                    m_remote_info[i].ptr = std::move(buffer);
                }
                //else
                //    m_mutex[i].unlock();
            }
        }
    }
}

void RDMA_Client::Stop()
{
    m_Runing = false;
    m_thd->join();
}

void RDMA_Client::Send(std::shared_ptr<RDMA_Message> buffer, int idx)
{
    //m_mutex[idx].lock();
    m_queue[idx].Push(buffer);
    //m_mutex[idx].unlock();
}

//* RDMA Server 
//
RDMA_Server::RDMA_Server(int max_client_num, int max_qp_num, const HandleMsg& Handle) 
    : RDMA_Device(max_client_num * max_qp_num)
{
    m_local_info_sz = 0;
    m_local_info = new RDMA_Local_Info[max_qp_num * max_client_num]();
    m_thd = new std::thread(&RDMA_Server::Recving, this, Handle);
}

RDMA_Server::~RDMA_Server()
{
    Stop();
    for (int i = 0; i < m_local_info_sz; ++i)
    {
        rdma_destroy_ep(m_local_info[i].local_cm_id);
        delete m_local_info[i].bufList;
    }

    delete [] m_local_info;
}

int RDMA_Server::Listen(const std::string& port, int blk_size, int queue_size)
{
    m_local_info[m_local_info_sz].local_cm_id = ResolveAddress(const_cast<char*>(port.c_str()), (char*)nullptr);
    if (rdma_listen(m_local_info[m_local_info_sz].local_cm_id, 100)) 
    {
        rdma_destroy_ep(m_local_info[m_local_info_sz].local_cm_id);
        throw std::system_error(errno, std::system_category());
    }
    m_local_info[m_local_info_sz].bufList = new BufferList(blk_size, queue_size);
    return m_local_info_sz++;
}

void RDMA_Server::Accept(int idx)
{
    if (rdma_get_request(m_local_info[idx].local_cm_id, &m_remote_info[m_sz].remote_cm_id))
    {
        throw std::system_error(errno, std::system_category());
    }

    rdma_conn_param params;
    memset(&params, 0, sizeof(params));
    params.initiator_depth = params.responder_resources = 1;
    params.rnr_retry_count = 7;
    //params.flow_control = idx + 1;
    if (rdma_accept(m_remote_info[m_sz].remote_cm_id, &params))
    {
        throw std::system_error(errno, std::system_category());
    }

    // if (SetQPTimeout(m_local_info[idx].local_cm_id->qp)) 
    // {
    //     throw std::system_error(errno, std::system_category());
    // }

    if (SetQPTimeout(m_remote_info[m_sz].remote_cm_id->qp)) 
    {
        throw std::system_error(errno, std::system_category());
    }
    
    RegistBuffer(m_sz, m_local_info[idx].bufList->Head(), m_local_info[idx].bufList->Capcity());
    m_remote_info[m_sz].buf = m_local_info[idx].bufList->GetFreeBuf();
    m_remote_info[m_sz].last_buf = m_local_info[idx].bufList->GetFreeBuf();
    m_remote_info[m_sz].idx = idx;
    if (rdma_post_recv(m_remote_info[m_sz].remote_cm_id, nullptr, (void *)m_remote_info[m_sz].buf, m_local_info[idx].bufList->Size(), m_remote_info[m_sz].mr))
    {
        throw std::system_error(errno, std::system_category());
    }
    /*
    auto addr = m_remote_info[m_sz].remote_cm_id->route.addr;
    printf("Src IP[%s], Port[%d]\n", inet_ntoa(((sockaddr_in *)&addr.src_addr)->sin_addr), ((sockaddr_in *)&addr.src_addr)->sin_port);
    printf("Dst IP[%s], Port[%d]\n", inet_ntoa(((sockaddr_in *)&addr.dst_addr)->sin_addr), ((sockaddr_in *)&addr.dst_addr)->sin_port);
    */
    ++m_sz;
}

void RDMA_Server::Stop()
{
    m_Runing = false;
    m_thd->join();
}

void RDMA_Server::Recving(const HandleMsg& Handle)
{
    std::shared_ptr<RDMA_Message> data;
    int idx;
    char* buf;

    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(30, &mask);
    if (sched_setaffinity(0, sizeof(mask), &mask) == -1)
    {
        throw std::system_error(errno, std::system_category());
    }
    
    puts("Server Start Receiving");
    //auto t1 = std::chrono::high_resolution_clock::now();
    while (m_Runing)
    {
        for (int i = 0; i < m_sz; ++i)
        {
            if (m_remote_info[i].last_buf)
            {
                if (PollCQ(m_remote_info[i].remote_cm_id->recv_cq))
                {
                    idx = m_remote_info[i].idx;
                    buf = m_remote_info[i].buf;
                    m_remote_info[i].buf = m_remote_info[i].last_buf;
                    rdma_post_recv(m_remote_info[i].remote_cm_id, nullptr, (void *)m_remote_info[i].buf, m_local_info[idx].bufList->Size(), m_remote_info[i].mr);
                    data.reset((RDMA_Message*)buf, [this, idx](RDMA_Message* msg){m_local_info[idx].bufList->FreeBuf((char *)msg);});
                    Handle(data);
                    m_remote_info[i].last_buf = m_local_info[idx].bufList->GetFreeBuf();
               	    /*auto t2 = std::chrono::high_resolution_clock::now();
                    auto diff = std::chrono::duration_cast<std::chrono::microseconds>(t2-t1);
                    if (diff.count()>1000) {
			printf("Outlier detected in %ld us\n", diff.count());
			std::system("ethtool -S eth1 | egrep '(r|t)x_pause_duration_prio_0'");
		    }
                    t1 = t2;*/
		}
            }
            else
            {
                idx = m_remote_info[i].idx;
                m_remote_info[i].last_buf = m_local_info[idx].bufList->GetFreeBuf();
            }
        }
    }
}


}
}
