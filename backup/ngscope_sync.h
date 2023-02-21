#ifndef NGSCOPE_DCI_HH
#define NGSCOPE_DCI_HH

#include "packet_list.h"

#define NOF_LOG_DCI 500
#define reTx_delay_thd 7
class SaturateServo;

typedef struct{
    uint64_t time_stamp;
    uint32_t tbs;
    uint8_t  reTx;
    uint16_t tti;
}ue_dci_t;

bool ngscope_sync_new_reTx(ue_dci_t* dci_vec,
                            int         last_reTx_idx,
                            int*        curr_reTx_idx,
                            int         header,
                            int         nof_dci);

long revert_last_pkt_reTx(packet_node* head,
                            long tbs);

int ngscope_sync_oneway_revert(packet_node* head,
                                int         nof_pkt,
                                uint64_t*   recv_t_us,
                                uint32_t*   tbs,
                                uint8_t*    reTx,
                                uint16_t*    tti,
                                long        tbs_in,
                                long*       tbs_out,
                                int         delay_in,
                                int*        delay_out,
                                int         last_reTx_tti,
                                int         offset,
                                int         header,
                                int         nof_dci,
                                FILE*       fd);

void ngscope_sync_extract_data_from_dci(ue_dci_t* dci_vec, 
                            uint64_t* recv_t_us, 
                            uint8_t* reTx, 
                            uint32_t* tbs, 
                            uint16_t* tti, 
                            int     header,
                            int     nof_dci);

int ngscope_sync_dci_delay(int* delay_diff_ms,
                            uint64_t* recv_t_us,
                            int list_len,
                            uint8_t* dci_reTx,
                            uint64_t* dci_recv_t_us,
                            int     nof_dci,
                            FILE* fd);
#endif
