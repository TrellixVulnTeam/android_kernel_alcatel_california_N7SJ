#include <asm/atomic.h>

#include "osal_typedef.h"
#include "osal.h"
#include "stp_dbg.h"
#include "stp_core.h"
#include "btm_core.h"
#include "wmt_plat.h"

#define PFX_BTM                         "[STP-BTM] "
#define STP_BTM_LOG_LOUD                 4
#define STP_BTM_LOG_DBG                  3
#define STP_BTM_LOG_INFO                 2
#define STP_BTM_LOG_WARN                 1
#define STP_BTM_LOG_ERR                  0

INT32 gBtmDbgLevel = STP_BTM_LOG_INFO;

#define STP_BTM_LOUD_FUNC(fmt, arg...)   if(gBtmDbgLevel >= STP_BTM_LOG_LOUD){ printk(KERN_DEBUG PFX_BTM "%s: "  fmt, __FUNCTION__ ,##arg);}
#define STP_BTM_DBG_FUNC(fmt, arg...)    if(gBtmDbgLevel >= STP_BTM_LOG_DBG){ printk(KERN_DEBUG PFX_BTM "%s: "  fmt, __FUNCTION__ ,##arg);}
#define STP_BTM_INFO_FUNC(fmt, arg...)   if(gBtmDbgLevel >= STP_BTM_LOG_INFO){ printk(PFX_BTM "[I]%s: "  fmt, __FUNCTION__ ,##arg);}
#define STP_BTM_WARN_FUNC(fmt, arg...)   if(gBtmDbgLevel >= STP_BTM_LOG_WARN){ printk(PFX_BTM "[W]%s: "  fmt, __FUNCTION__ ,##arg);}
#define STP_BTM_ERR_FUNC(fmt, arg...)    if(gBtmDbgLevel >= STP_BTM_LOG_ERR){  printk(PFX_BTM "[E]%s(%d):ERROR! "   fmt, __FUNCTION__ , __LINE__, ##arg);}
#define STP_BTM_TRC_FUNC(f)              if(gBtmDbgLevel >= STP_BTM_LOG_DBG){ printk(KERN_DEBUG PFX_BTM "<%s> <%d>\n", __FUNCTION__, __LINE__);}


#define ASSERT(expr)

MTKSTP_BTM_T stp_btm_i;
MTKSTP_BTM_T *stp_btm = &stp_btm_i;

const char *g_btm_op_name[]={
        "STP_OPID_BTM_RETRY",
        "STP_OPID_BTM_RST",
        "STP_OPID_BTM_DBG_DUMP",
        "STP_OPID_BTM_DUMP_TIMEOUT",
	    "STP_OPID_BTM_POLL_CPUPCR",
	    "STP_OPID_BTM_PAGED_DUMP",
	    "STP_OPID_BTM_FULL_DUMP",
	    "STP_OPID_BTM_PAGED_TRACE",
	    "STP_OPID_BTM_FORCE_FW_ASSERT",
	    "STP_OPID_BTM_EXIT"
    };

#if 0
static char *_stp_pkt_type(int type){

    static char s[10]; 

    switch(type){
        case WMT_TASK_INDX:
            osal_memcpy(s, "WMT", strlen("WMT")+1);
            break;
        case BT_TASK_INDX:
            osal_memcpy(s, "BT", strlen("BT")+1);
            break;
        case GPS_TASK_INDX:
            osal_memcpy(s, "GPS", strlen("GPS")+1);
            break;
        case FM_TASK_INDX:
            osal_memcpy(s, "FM", strlen("FM")+1);
            break;
        default:
            osal_memcpy(s, "UNKOWN", strlen("UNKOWN")+1);
            break;
    }

    return s;
}
#endif 

static INT32 _stp_btm_put_dump_to_nl(void)
{
    #define NUM_FETCH_ENTRY 8   

    static UINT8  buf[2048];
    static UINT8  tmp[2048];

    UINT32  buf_len;       
    STP_PACKET_T  *pkt;
    STP_DBG_HDR_T *hdr;
    INT32  remain=0, index =0;
    INT32 retry = 0, rc = 0, nl_retry = 0;
    STP_BTM_INFO_FUNC("Enter..\n");
  
    index = 0;
    tmp[index++]='[';
    tmp[index++]='M';
    tmp[index++]=']'; 

    do
    {
        index = 3;
        remain = stp_dbg_dmp_out_ex(&buf[0], &buf_len);
        if (buf_len > 0)
        {
            pkt = (STP_PACKET_T  *)buf;
            hdr = &pkt->hdr;
            if (hdr->dbg_type == STP_DBG_FW_DMP){
                memcpy(&tmp[index], pkt->raw, pkt->hdr.len);

                if(pkt->hdr.len <= 1500)
                {
                    tmp[index + pkt->hdr.len] = '\n';
                    tmp[index + pkt->hdr.len + 1] = '\0';

                    //printk("\n%s\n+++\n", tmp);
                    rc = stp_dbg_nl_send((char *)&tmp, 2);

                    while(rc){
                       nl_retry++;                       
                       if(nl_retry > 1000){
                            break;
                       }                       
                       STP_BTM_WARN_FUNC("**dump send fails, and retry again.**\n");
                       msleep(3);
                       rc = stp_dbg_nl_send((char *)&tmp, 2);
                       if(!rc){
                          STP_BTM_WARN_FUNC("****retry again ok!**\n");
                       }
                    }                    
                    //schedule();
                } else {
                    STP_BTM_INFO_FUNC("dump entry length is over long\n");
                    BUG_ON(0);
                }
                retry = 0;
            }
        }else
        {
            retry ++;
            msleep(100);
        }
    }while((remain > 0) || (retry < 2));

    STP_BTM_INFO_FUNC("Exit..\n");
    return 0;
}


static INT32 _stp_btm_put_dump_to_aee(void)
{
    static UINT8  buf[2048];
    static UINT8  tmp[2048];

    UINT32  buf_len;       
    STP_PACKET_T  *pkt;
    STP_DBG_HDR_T *hdr;
    INT32 remain = 0;
    INT32 retry = 0;
    INT32 ret = 0;
    STP_BTM_INFO_FUNC("Enter..\n");

    do {
        remain = stp_dbg_dmp_out_ex(&buf[0], &buf_len);
        if (buf_len > 0) {
            pkt = (STP_PACKET_T*)buf;
            hdr = &pkt->hdr;
            if (hdr->dbg_type == STP_DBG_FW_DMP) {
                memcpy(&tmp[0], pkt->raw, pkt->hdr.len);

                if (pkt->hdr.len <= 1500) {
                    tmp[pkt->hdr.len] = '\n';
                    tmp[pkt->hdr.len + 1] = '\0';

                    ret = stp_dbg_aee_send(tmp, pkt->hdr.len, 0);                 
                } else {
                    STP_BTM_INFO_FUNC("dump entry length is over long\n");
                    BUG_ON(0);
                }
                retry = 0;
            }
        } else {  
            retry ++;
            msleep(100);
        }
    }while ((remain > 0) || (retry < 2));

    STP_BTM_INFO_FUNC("Exit..\n");
    return ret;
}

static INT32 _stp_set_host_dump_state(ENUM_HOST_DUMP_STATE state)
{
	UINT8 * p_virtual_addr = NULL;

	p_virtual_addr = wmt_plat_get_emi_ctrl_state_base_add(EXP_APMEM_CTRL_HOST_SYNC_STATE);
	if(!p_virtual_addr)
	{
		STP_BTM_ERR_FUNC("get virtual address fail\n");
		return -1;
	}

	CONSYS_REG_WRITE(p_virtual_addr, state);

	return 0;
}


INT32 _stp_trigger_firmware_assert_via_emi(VOID)
{
	UINT8 * p_virtual_addr = NULL;
    INT32 status = -1;
   	INT32 i = 0, j = 0;
	
    do {        
        STP_BTM_INFO_FUNC("[Force Assert] stp_trigger_firmware_assert_via_emi -->\n");
    	p_virtual_addr = wmt_plat_get_emi_ctrl_state_base_add(EXP_APMEM_CTRL_HOST_OUTBAND_ASSERT_W1);
    	if(!p_virtual_addr)
    	{
    		STP_BTM_ERR_FUNC("get virtual address fail\n");
    		return -1;
    	}

    	CONSYS_REG_WRITE(p_virtual_addr, EXP_APMEM_HOST_OUTBAND_ASSERT_MAGIC_W1);
        STP_BTM_INFO_FUNC("[Force Assert] stp_trigger_firmware_assert_via_emi <--\n");
#if 1
        //wait for firmware assert 
        osal_msleep(50); 
        //if firmware is not assert self, host driver helps it.
        do
        {
            if(0 != mtk_wcn_stp_coredump_start_get()){
                status = 0;
                break;
            }  
			
            mtk_wcn_stp_wakeup_consys();
            STP_BTM_INFO_FUNC("[Force Assert] wakeup consys (%d)\n", i);
            stp_dbg_poll_cpupcr(5 , 1 , 1);
            osal_msleep(5);

			i++;
            if(i > 20){
				i = 0;
                break;
            }          
        } while(1);
#endif

        if(0 != mtk_wcn_stp_coredump_start_get()){
            status = 0;
            break;
        }

		j++;
        if(j > 8) {
			j = 0;
            break;        
        }
    } while(1);
    
    return status;
}

static INT32 _stp_update_host_sync_num(VOID)
{
	UINT8 * p_virtual_addr = NULL;
	UINT32 sync_num = 0;
	
	p_virtual_addr = wmt_plat_get_emi_ctrl_state_base_add(EXP_APMEM_CTRL_HOST_SYNC_NUM);
	if(!p_virtual_addr)
	{
		STP_BTM_ERR_FUNC("get virtual address fail\n");
		return -1;
	}

	sync_num = CONSYS_REG_READ(p_virtual_addr);
	CONSYS_REG_WRITE(p_virtual_addr, sync_num + 1);

	return 0;
}
static INT32 _stp_get_dump_info(ENUM_EMI_CTRL_STATE_OFFSET offset)
{
	UINT8 * p_virtual_addr = NULL;

	p_virtual_addr = wmt_plat_get_emi_ctrl_state_base_add(offset);
	if(!p_virtual_addr)
	{
		STP_BTM_ERR_FUNC("get virtual address fail\n");
		return -1;
	}

	return CONSYS_REG_READ(p_virtual_addr);
}
static INT32 _stp_set_dump_info(ENUM_EMI_CTRL_STATE_OFFSET offset,UINT32 value)
{
	UINT8 * p_virtual_addr = NULL;

	p_virtual_addr = wmt_plat_get_emi_ctrl_state_base_add(offset);
	if(!p_virtual_addr)
	{
		STP_BTM_ERR_FUNC("get virtual address fail\n");
		return -1;
	}
	CONSYS_REG_WRITE(p_virtual_addr,value);
	return 0;
}

#define COMBO_DUMP2AEE
#if 1
#define STP_DBG_PAGED_DUMP_BUFFER_SIZE (32*1024*sizeof(char))
UINT8 g_paged_dump_buffer[STP_DBG_PAGED_DUMP_BUFFER_SIZE]={0};
#define STP_DBG_PAGED_TRACE_SIZE (2048*sizeof(char))
UINT8 g_paged_trace_buffer[STP_DBG_PAGED_TRACE_SIZE] = {0};
UINT32 g_paged_dump_len = 0;
UINT32 g_paged_trace_len = 0;
VOID _stp_dump_emi_dump_buffer(UINT8 *buffer,UINT32 len)
{
	UINT32 i = 0;
	if(len > 16)
		len  = 16;
	for(i = 0;i < len;i++)
	{
		if(i%16 == 0 && i != 0)  printk("\n    ");

        if(buffer[i]==']' || buffer[i]=='[' || buffer[i]==','){
            printk("%c",  buffer[i]);
        }else {
            printk("0x%02x ",  buffer[i]);
        }
   }
}
#endif
static INT32 _stp_btm_handler(MTKSTP_BTM_T *stp_btm, P_STP_BTM_OP pStpOp)
{
    INT32 ret = -1;
    INT32 dump_sink = 1; //core dump target, 0: aee; 1: netlink
    static UINT32 counter = 0;
	UINT32 full_dump_left = STP_FULL_DUMP_TIME;
	UINT32 page_counter = 0;
	ENUM_STP_FW_ISSUE_TYPE issue_type;
	
    if (NULL == pStpOp) 
    {
        return -1;
    }

    switch(pStpOp->opId) 
    {
        case STP_OPID_BTM_EXIT:
            // TODO: clean all up?
            ret = 0;
        break;

        /*tx timeout retry*/
        case STP_OPID_BTM_RETRY:
            stp_do_tx_timeout();
            ret = 0;
            
        break;

        /*whole chip reset*/
        case STP_OPID_BTM_RST:
            STP_BTM_INFO_FUNC("whole chip reset start!\n");
            STP_BTM_INFO_FUNC("....+\n");
            if(stp_btm->wmt_notify)
            {
                stp_btm->wmt_notify(BTM_RST_OP);
                ret = 0;
            }
            else
            {
                STP_BTM_ERR_FUNC("stp_btm->wmt_notify is NULL.");
                ret = -1;
            }

            STP_BTM_INFO_FUNC("whole chip reset end!\n");
            
        break;

        case STP_OPID_BTM_DBG_DUMP:
            /*Notify the wmt to get dump data*/
            STP_BTM_DBG_FUNC("wmt dmp notification\n");
            dump_sink = ((stp_btm->wmt_notify(BTM_GET_AEE_SUPPORT_FLAG) == MTK_WCN_BOOL_TRUE) ? 0 : 1);
            
            if (dump_sink == 0) {
                _stp_btm_put_dump_to_aee();
            } else if (dump_sink == 1) {
                _stp_btm_put_dump_to_nl();
            } else {
                STP_BTM_ERR_FUNC("unknown sink %d\n", dump_sink);
            }            
                           
        break;


        case STP_OPID_BTM_DUMP_TIMEOUT:
            // Flush dump data, and reset compressor
            STP_BTM_INFO_FUNC("Flush dump data\n");
            wcn_core_dump_flush(0);
        break;

		case STP_OPID_BTM_POLL_CPUPCR:
			do{
				UINT32 times;
				UINT32 sleep;

				times = pStpOp->au4OpData[0];
				sleep = pStpOp->au4OpData[1];

				ret = stp_dbg_poll_cpupcr(times, sleep,0);
			}while(0);
		break;
		
		case STP_OPID_BTM_PAGED_DUMP:
		g_paged_dump_len = 0;
		issue_type = STP_FW_ASSERT_ISSUE;
		
		_stp_set_host_dump_state(STP_HOST_DUMP_NOT_START);
		page_counter = 0;
		do{
			UINT32 loop_cnt1 = 0;
			UINT32 loop_cnt2 = 0;
			ENUM_HOST_DUMP_STATE host_state;
			ENUM_CHIP_DUMP_STATE chip_state;
			UINT32 dump_phy_addr = 0;
			UINT8 *dump_vir_addr = NULL;
			UINT32 dump_len = 0;
			UINT32 isEnd = 0;
			
			host_state = (ENUM_HOST_DUMP_STATE)_stp_get_dump_info(EXP_APMEM_CTRL_HOST_SYNC_STATE);
			if(STP_HOST_DUMP_NOT_START == host_state)
			{
				counter++;
				STP_BTM_INFO_FUNC("counter(%d)\n",counter);
				osal_msleep(100);
			}
			else
			{
				counter = 0;
			}
			while(1)
			{
				chip_state = (ENUM_CHIP_DUMP_STATE)_stp_get_dump_info(EXP_APMEM_CTRL_CHIP_SYNC_STATE);
				if(STP_CHIP_DUMP_PUT_DONE == chip_state){
                    STP_BTM_INFO_FUNC("chip put done\n");
                    break;
                }    
				else
				{
				    STP_BTM_INFO_FUNC("waiting chip put done\n");
					loop_cnt1 ++;
					osal_msleep(5);
				}
				if(loop_cnt1 > 10)
					goto paged_dump_end;
				
			}

			_stp_set_host_dump_state(STP_HOST_DUMP_GET);

			dump_phy_addr = _stp_get_dump_info(EXP_APMEM_CTRL_CHIP_SYNC_ADDR);
			
			if(!dump_phy_addr)
			{
				STP_BTM_ERR_FUNC("get paged dump phy address fail\n");
				ret = -1;
				break;
			}

			dump_vir_addr = wmt_plat_get_emi_ctrl_state_base_add(dump_phy_addr - CONSYS_EMI_FW_PHY_BASE);
			if(!dump_vir_addr)
			{
				STP_BTM_ERR_FUNC("get paged dump phy address fail\n");
				ret = -2;
				break;
			}
			dump_len = _stp_get_dump_info(EXP_APMEM_CTRL_CHIP_SYNC_LEN);
			STP_BTM_INFO_FUNC("dump_phy_ddr(%08x),dump_vir_add(0x%p),dump_len(%d)\n",dump_phy_addr,dump_vir_addr,dump_len);

			/*move dump info according to dump_addr & dump_len*/
#if 1
			osal_memcpy(&g_paged_dump_buffer[0],dump_vir_addr,dump_len);
			_stp_dump_emi_dump_buffer(&g_paged_dump_buffer[0],dump_len);

			if(0 == page_counter)//do fw assert infor paser in first paged dump
			{
				if(1 == stp_dbg_get_host_trigger_assert())
				{
					issue_type = STP_HOST_TRIGGER_FW_ASSERT;
				}
				ret = stp_dbg_set_fw_info(&g_paged_dump_buffer[0],512,issue_type);
				if(ret)
				{
					WMT_ERR_FUNC("set fw issue infor fail(%d),maybe fw warm reset...\n",ret);
					stp_dbg_set_fw_info("Fw Warm reset",osal_strlen("Fw Warm reset"),STP_FW_WARM_RST_ISSUE);
				}
			}
			
            if(dump_len <= 32*1024){
                ret = stp_dbg_aee_send(&g_paged_dump_buffer[0],dump_len, 0);
                if(ret == 0){
                     STP_BTM_INFO_FUNC("aee send ok!\n");
                } else if (ret == 1) {
                     STP_BTM_INFO_FUNC("aee send fisish!\n");
                } else {
                     STP_BTM_ERR_FUNC("aee send error!\n");
                }
            } else {
                STP_BTM_ERR_FUNC("dump len is over than 32K(%d)\n",dump_len);
            }
            
            g_paged_dump_len += dump_len;
			STP_BTM_INFO_FUNC("dump len update(%d)\n",g_paged_dump_len);
#endif
			_stp_update_host_sync_num();
			_stp_set_host_dump_state(STP_HOST_DUMP_GET_DONE);

			STP_BTM_INFO_FUNC("host sync num(%d),chip sync num(%d)\n",
				_stp_get_dump_info(EXP_APMEM_CTRL_HOST_SYNC_NUM),
				_stp_get_dump_info(EXP_APMEM_CTRL_CHIP_SYNC_NUM));

            page_counter++;
            STP_BTM_INFO_FUNC("\n\n++ paged dump counter(%d) ++\n\n\n",page_counter);
            
			while(1)
			{
				chip_state = (ENUM_CHIP_DUMP_STATE)_stp_get_dump_info(EXP_APMEM_CTRL_CHIP_SYNC_STATE);
				if(STP_CHIP_DUMP_END == chip_state)
				{
				    STP_BTM_INFO_FUNC("chip put end\n");
					_stp_set_host_dump_state(STP_HOST_DUMP_END);
					break;
				}
				else
				{
                    STP_BTM_INFO_FUNC("waiting chip put end\n");
                
					loop_cnt2++;
					osal_msleep(10);
				}
				if(loop_cnt2 > 10)
					goto paged_dump_end;
			}
			
paged_dump_end:
			_stp_set_host_dump_state(STP_HOST_DUMP_NOT_START);
			
            if(counter * 100 > STP_PAGED_DUMP_TIME_LIMIT)
            {
    			isEnd = _stp_get_dump_info(EXP_APMEM_CTRL_CHIP_PAGED_DUMP_END);

                if(isEnd)
    			{
    				STP_BTM_INFO_FUNC("paged dump end\n");

                    STP_BTM_INFO_FUNC("\n\n paged dump print  ++ \n\n");
                    _stp_dump_emi_dump_buffer(&g_paged_dump_buffer[0],g_paged_dump_len);
                    STP_BTM_INFO_FUNC("\n\n paged dump print  -- \n\n");
                    STP_BTM_INFO_FUNC("\n\n paged dump size = %d, paged dump page number = %d \n\n", g_paged_dump_len, page_counter);
                    counter = 0;
    				ret = 0;
    				break;
     			} 
                else
                {    			
                    STP_BTM_ERR_FUNC("paged dump fail\n");
                    _stp_set_host_dump_state(STP_HOST_DUMP_NOT_START);
					stp_dbg_poll_cpupcr(5,5,0);
					counter = 0;
                    ret = -1;
                    break;
                }
            }

		}while(1);
        
        break;

		case STP_OPID_BTM_FULL_DUMP:

		_stp_set_host_dump_state(STP_HOST_DUMP_NOT_START);
		do{
			UINT32 loop_cnt1 = 0;
			UINT32 loop_cnt2 = 0;
			ENUM_HOST_DUMP_STATE host_state;
			ENUM_CHIP_DUMP_STATE chip_state;
			UINT32 dump_phy_addr = 0;
			UINT8 *dump_vir_addr = NULL;
			UINT32 dump_len = 0;
			UINT32 isFail = 0;
			
			while(1)
			{
				chip_state = (ENUM_CHIP_DUMP_STATE)_stp_get_dump_info(EXP_APMEM_CTRL_CHIP_SYNC_STATE);
				if(STP_CHIP_DUMP_PUT_DONE == chip_state)
					break;
				else
				{
					loop_cnt1 ++;
					osal_msleep(10);
				}
				if(loop_cnt1 > 10)
				{
					isFail = 1;
					goto full_dump_end;
				}
			}

			_stp_set_host_dump_state(STP_HOST_DUMP_GET);

			dump_phy_addr = _stp_get_dump_info(EXP_APMEM_CTRL_CHIP_SYNC_ADDR);
			if(!dump_phy_addr)
			{
				STP_BTM_ERR_FUNC("get phy dump address fail\n");
				ret = -1;
				break;
			}
			
			dump_vir_addr = wmt_plat_get_emi_ctrl_state_base_add(dump_phy_addr - CONSYS_EMI_FW_PHY_BASE);
			if(!dump_vir_addr)
			{
				STP_BTM_ERR_FUNC("get vir dump address fail\n");
				ret = -2;
				break;
			}
			dump_len = _stp_get_dump_info(EXP_APMEM_CTRL_CHIP_SYNC_LEN);
			/*move dump info according to dump_addr & dump_len*/
			_stp_update_host_sync_num();
			_stp_set_host_dump_state(STP_HOST_DUMP_GET_DONE);

			STP_BTM_INFO_FUNC("host sync num(%d),chip sync num(%d)\n",
				_stp_get_dump_info(EXP_APMEM_CTRL_HOST_SYNC_NUM),
				_stp_get_dump_info(EXP_APMEM_CTRL_CHIP_SYNC_NUM));
			
			while(1)
			{
				chip_state = (ENUM_CHIP_DUMP_STATE)_stp_get_dump_info(EXP_APMEM_CTRL_CHIP_SYNC_STATE);
				if(STP_CHIP_DUMP_END == chip_state)
				{
					_stp_set_host_dump_state(STP_HOST_DUMP_END);
					break;
				}
				else
				{
					loop_cnt2++;
					osal_msleep(10);
				}
				if(loop_cnt2 > 10)
				{
					isFail = 1;
					goto full_dump_end;
				}
			}
			_stp_set_host_dump_state(STP_HOST_DUMP_NOT_START);
full_dump_end:
			if(isFail)
			{
				STP_BTM_ERR_FUNC("full dump fail\n");
				_stp_set_host_dump_state(STP_HOST_DUMP_NOT_START);
				ret = -1;
				break;
			}
		}while(--full_dump_left > 0);
		if(0 == full_dump_left)
		{
			STP_BTM_INFO_FUNC("full dump end\n");
			ret = 0;
		}
		break;
		case STP_OPID_BTM_PAGED_TRACE:
		g_paged_trace_len = 0;
		do{
			UINT32 ctrl_val = 0;
			UINT32 loop_cnt1 = 0;
			UINT32 buffer_start = 0;
			UINT32 buffer_idx = 0;
			UINT32 buffer_len  = 0;
			UINT8 *dump_vir_addr = NULL; 
			
			while(loop_cnt1 < 10)
			{
				ctrl_val = _stp_get_dump_info(EXP_APMEM_CTRL_STATE);
				if(0x8 == ctrl_val)
					break;
				else
				{
					osal_msleep(10);
					loop_cnt1++;
				}
			}

			if(loop_cnt1 >= 10)
			{
				STP_BTM_ERR_FUNC("polling CTRL STATE fail\n");
				ret = -1;
				break;
			}

			buffer_start = _stp_get_dump_info(EXP_APMEM_CTRL_CHIP_PRINT_BUFF_START);
			buffer_idx = _stp_get_dump_info(EXP_APMEM_CTRL_CHIP_PRINT_BUFF_IDX);
			//buffer_len = buffer_idx - buffer_start;
			g_paged_trace_len = buffer_idx;
			STP_BTM_INFO_FUNC("paged trace buffer addr(%08x),buffer_len(%d)\n",buffer_start,buffer_idx);
			dump_vir_addr = wmt_plat_get_emi_ctrl_state_base_add(buffer_start - CONSYS_EMI_FW_PHY_BASE);
			if(!dump_vir_addr)
			{
				STP_BTM_ERR_FUNC("get vir dump address fail\n");
				ret = -2;
				break;
			}
			osal_memcpy(&g_paged_trace_buffer[0],dump_vir_addr,buffer_idx < STP_DBG_PAGED_TRACE_SIZE ? buffer_idx : STP_DBG_PAGED_TRACE_SIZE);
			/*moving paged trace according to buffer_start & buffer_len*/
            do {
                int i = 0;
				int dump_len = 0;
				dump_len = buffer_idx < STP_DBG_PAGED_TRACE_SIZE ? buffer_idx : STP_DBG_PAGED_TRACE_SIZE;
                printk("\n\n -- paged trace hex output --\n\n");
                for(i = 0;i < dump_len; i++)
                {
                    if(i%16 == 0){
                    printk("\n");
                    }
                    printk("%02x ",g_paged_trace_buffer[i]);
                }
                printk("\n\n -- paged trace ascii output --\n\n");	
                for(i = 0;i < dump_len; i++)
                {
                    if(i%64 == 0)
                    printk("\n");
                    printk("%c",g_paged_trace_buffer[i]);
                }
            }while(0);
			/*move parser fw assert infor to paged dump in the one paged dump*/
			//ret = stp_dbg_set_fw_info(&g_paged_trace_buffer[0],g_paged_trace_len,issue_type);
			ret = 0;

		}while(0);
		mtk_wcn_stp_ctx_restore();
		break;
#if 0
		case STP_OPID_BTM_FORCE_FW_ASSERT:
			ret = _stp_trigger_firmware_assert_via_emi();
			if(0 != _stp_get_dump_info(EXP_APMEM_CTRL_HOST_OUTBAND_ASSERT_W1))
			{
				STP_BTM_INFO_FUNC("EXP_APMEM_CTRL_HOST_OUTBAND_ASSERT_W1 has not clear,reset it\n");
				_stp_set_dump_info(EXP_APMEM_CTRL_HOST_OUTBAND_ASSERT_W1, 0x0);
			}
			else
			{
				STP_BTM_INFO_FUNC("EXP_APMEM_CTRL_HOST_OUTBAND_ASSERT_W1 reset by fw side\n");
			}
		break;
#endif
        default:
            ret = -1;
        break;
    }
    
    return ret;
}


static P_OSAL_OP _stp_btm_get_op (
    MTKSTP_BTM_T *stp_btm,
    P_OSAL_OP_Q pOpQ
    )
{
    P_OSAL_OP pOp;
    //INT32 ret = 0;

    if (!pOpQ) 
    {
        STP_BTM_WARN_FUNC("!pOpQ \n");
        return NULL;
    }
    
    osal_lock_unsleepable_lock(&(stp_btm->wq_spinlock));
    /* acquire lock success */
    RB_GET(pOpQ, pOp);
    osal_unlock_unsleepable_lock(&(stp_btm->wq_spinlock));

    if (!pOp) 
    {
        //STP_BTM_WARN_FUNC("RB_GET fail\n");
    }

    return pOp;
}

static INT32 _stp_btm_put_op (
    MTKSTP_BTM_T *stp_btm,
    P_OSAL_OP_Q pOpQ,
    P_OSAL_OP pOp
    )
{
    INT32 ret;

    if (!pOpQ || !pOp) 
    {
        STP_BTM_WARN_FUNC("invalid input param: 0x%p, 0x%p \n", pOpQ, pOp);
        return 0;//;MTK_WCN_BOOL_FALSE;
    }

    ret = 0;

    osal_lock_unsleepable_lock(&(stp_btm->wq_spinlock));
    /* acquire lock success */
    if (!RB_FULL(pOpQ)) 
    {
        RB_PUT(pOpQ, pOp);
    }
    else 
    {
        ret = -1;
    }
    osal_unlock_unsleepable_lock(&(stp_btm->wq_spinlock));
    
    if (ret) 
    {
        STP_BTM_WARN_FUNC("RB_FULL(0x%p) %d ,rFreeOpQ = %p, rActiveOpQ = %p\n", pOpQ, RB_COUNT(pOpQ),
            &stp_btm->rFreeOpQ,  &stp_btm->rActiveOpQ);
        return 0; 
    }
    else 
    {
        //STP_BTM_WARN_FUNC("RB_COUNT = %d\n",RB_COUNT(pOpQ));
        return 1; 
    }
}

P_OSAL_OP _stp_btm_get_free_op (
    MTKSTP_BTM_T *stp_btm
    )
{
    P_OSAL_OP pOp;

    if (stp_btm) 
    {
        pOp = _stp_btm_get_op(stp_btm, &stp_btm->rFreeOpQ);
        if (pOp) 
        {
            osal_memset(&pOp->op, 0, sizeof(pOp->op));
        }
        return pOp;
    }
    else {
        return NULL;
    }
}

INT32 _stp_btm_put_act_op (
    MTKSTP_BTM_T *stp_btm,
    P_OSAL_OP pOp
    )
{
    INT32 bRet = 0;
    INT32 bCleanup = 0;
    long wait_ret = -1;

    P_OSAL_SIGNAL pSignal = NULL;

    do {
        if (!stp_btm || !pOp) 
        {
            break;
        }

        pSignal = &pOp->signal;
        
        if (pSignal->timeoutValue) 
        {
            pOp->result = -9;
            osal_signal_init(&pOp->signal);
        }

        /* put to active Q */
        bRet = _stp_btm_put_op(stp_btm, &stp_btm->rActiveOpQ, pOp);
        if(0 == bRet)
        {
            STP_BTM_WARN_FUNC("put active queue fail\n");
            bCleanup = 1;//MTK_WCN_BOOL_TRUE;
            break;
        }
        
        /* wake up wmtd */
        osal_trigger_event(&stp_btm->STPd_event);

        if (pSignal->timeoutValue == 0) 
        {
            bRet = 1;//MTK_WCN_BOOL_TRUE;
            /* clean it in wmtd */
            break;
        }
        
        /* wait result, clean it here */
        bCleanup = 1;//MTK_WCN_BOOL_TRUE;

        /* check result */
        wait_ret = osal_wait_for_signal_timeout(&pOp->signal);

        STP_BTM_DBG_FUNC("wait completion:%ld\n", wait_ret);
        if (!wait_ret) 
        {
            STP_BTM_ERR_FUNC("wait completion timeout \n");
            // TODO: how to handle it? retry?
        }
        else 
        {
            if (pOp->result) 
            {
                STP_BTM_WARN_FUNC("op(%d) result:%d\n", pOp->op.opId, pOp->result);
            }
            
            bRet = (pOp->result) ? 0 : 1;
        }
    } while(0);

    if (bCleanup) {
        /* put Op back to freeQ */
        _stp_btm_put_op(stp_btm, &stp_btm->rFreeOpQ, pOp);
    }
	bRet = (pOp->result) ? 0 : 1;
    return bRet;
}

static INT32 _stp_btm_wait_for_msg(void *pvData)
{
    MTKSTP_BTM_T *stp_btm = (MTKSTP_BTM_T *)pvData;
    return ((!RB_EMPTY(&stp_btm->rActiveOpQ)) || osal_thread_should_stop(&stp_btm->BTMd));
}

static INT32 _stp_btm_proc (void *pvData)
{
    MTKSTP_BTM_T *stp_btm = (MTKSTP_BTM_T *)pvData;
    P_OSAL_OP pOp;
    INT32 id;
    INT32 result;
    
    if (!stp_btm) 
    {
        STP_BTM_WARN_FUNC("!stp_btm \n");
        return -1;
    }

    for (;;) 
    {
        pOp = NULL;
        
        osal_wait_for_event(&stp_btm->STPd_event, 
            _stp_btm_wait_for_msg,
            (void *)stp_btm
            );

        if (osal_thread_should_stop(&stp_btm->BTMd)) 
        {
            STP_BTM_INFO_FUNC("should stop now... \n");
            // TODO: clean up active opQ
            break;
        }

        /* get Op from activeQ */
        pOp = _stp_btm_get_op(stp_btm, &stp_btm->rActiveOpQ);

        if (!pOp) 
        {
            STP_BTM_WARN_FUNC("get_lxop activeQ fail\n");
            continue;
        }

        id = osal_op_get_id(pOp);

        STP_BTM_DBG_FUNC("======> lxop_get_opid = %d, %s, remaining count = *%d*\n",
            id, (id >= 10)?("???"):(g_btm_op_name[id]), RB_COUNT(&stp_btm->rActiveOpQ));

        if (id >= STP_OPID_BTM_NUM) 
        {
            STP_BTM_WARN_FUNC("abnormal opid id: 0x%x \n", id);
            result = -1;
            goto handler_done;
        }
        
        result = _stp_btm_handler(stp_btm, &pOp->op);

handler_done:

        if (result) 
        {
            STP_BTM_WARN_FUNC("opid id(0x%x)(%s) error(%d)\n", id, (id >= 10)?("???"):(g_btm_op_name[id]), result);
        }

        if (osal_op_is_wait_for_signal(pOp)) 
        {
            osal_op_raise_signal(pOp, result);
        }
        else 
        {
            /* put Op back to freeQ */
            _stp_btm_put_op(stp_btm, &stp_btm->rFreeOpQ, pOp);
        }
        
        if (STP_OPID_BTM_EXIT == id) 
        {
            break;
        } else if (STP_OPID_BTM_RST == id) {
            /* prevent multi reset case */
            stp_btm_reset_btm_wq(stp_btm); 
        }
    }
    
    STP_BTM_INFO_FUNC("exits \n");

    return 0;
};

static inline INT32 _stp_btm_notify_wmt_rst_wq(MTKSTP_BTM_T *stp_btm)
{

    P_OSAL_OP       pOp;
    INT32           bRet;
    INT32 retval;

    if(stp_btm == NULL)
    {
        return STP_BTM_OPERATION_FAIL;
    }
    else 
    {
        pOp = _stp_btm_get_free_op(stp_btm);
        if (!pOp) 
        {
            STP_BTM_WARN_FUNC("get_free_lxop fail \n");
            return -1;//break;
        }
        pOp->op.opId = STP_OPID_BTM_RST;
        pOp->signal.timeoutValue= 0;
        bRet = _stp_btm_put_act_op(stp_btm, pOp);
        STP_BTM_DBG_FUNC("OPID(%d) type(%d) bRet(%d) \n\n",
            pOp->op.opId,
            pOp->op.au4OpData[0],
            bRet);
        retval = (0 == bRet) ? STP_BTM_OPERATION_FAIL : STP_BTM_OPERATION_SUCCESS;
    }
    return retval;
}

static inline INT32 _stp_btm_notify_stp_retry_wq(MTKSTP_BTM_T *stp_btm){

    P_OSAL_OP       pOp;
    INT32           bRet;
    INT32 retval;

    if(stp_btm == NULL)
    {
        return STP_BTM_OPERATION_FAIL;
    }
    else 
    {
        pOp = _stp_btm_get_free_op(stp_btm);
        if (!pOp) 
        {
            STP_BTM_WARN_FUNC("get_free_lxop fail \n");
            return -1;//break;
        }
        pOp->op.opId = STP_OPID_BTM_RETRY;
        pOp->signal.timeoutValue= 0;
        bRet = _stp_btm_put_act_op(stp_btm, pOp);
        STP_BTM_DBG_FUNC("OPID(%d) type(%d) bRet(%d) \n\n",
            pOp->op.opId,
            pOp->op.au4OpData[0],
            bRet);
        retval = (0 == bRet) ? STP_BTM_OPERATION_FAIL : STP_BTM_OPERATION_SUCCESS;
    }
    return retval;
}


static inline INT32 _stp_btm_notify_coredump_timeout_wq(MTKSTP_BTM_T *stp_btm){

    P_OSAL_OP       pOp;
    INT32           bRet;
    INT32 retval;

    if (!stp_btm) {
        return STP_BTM_OPERATION_FAIL;
    } else {
        pOp = _stp_btm_get_free_op(stp_btm);
        if (!pOp) {
            STP_BTM_WARN_FUNC("get_free_lxop fail \n");
            return -1;//break;
        }
        pOp->op.opId = STP_OPID_BTM_DUMP_TIMEOUT;
        pOp->signal.timeoutValue= 0;
        bRet = _stp_btm_put_act_op(stp_btm, pOp);
        STP_BTM_DBG_FUNC("OPID(%d) type(%d) bRet(%d) \n\n",
            pOp->op.opId,
            pOp->op.au4OpData[0],
            bRet);
        retval = (0 == bRet) ? STP_BTM_OPERATION_FAIL : STP_BTM_OPERATION_SUCCESS;
    }
    return retval;
}

static inline INT32 _stp_btm_dump_type(MTKSTP_BTM_T *stp_btm,ENUM_STP_BTM_OPID_T opid)
{
	P_OSAL_OP     pOp;
    INT32         bRet;
    INT32 retval;
	
	pOp = _stp_btm_get_free_op(stp_btm);
    if (!pOp) 
    {
        STP_BTM_WARN_FUNC("get_free_lxop fail \n");
        return -1;//break;
    }

	pOp->op.opId = opid;
	pOp->signal.timeoutValue= 0;
    bRet = _stp_btm_put_act_op(stp_btm, pOp);
    STP_BTM_DBG_FUNC("OPID(%d) type(%d) bRet(%d) \n\n",
    	pOp->op.opId,
    	pOp->op.au4OpData[0],
        bRet);
    retval = (0 == bRet) ? STP_BTM_OPERATION_FAIL : STP_BTM_OPERATION_SUCCESS;

	return retval;
}
static inline INT32 _stp_btm_notify_wmt_dmp_wq(MTKSTP_BTM_T *stp_btm){

    INT32 retval;
	UINT32 dump_type;
	UINT8 *virtual_addr = NULL;
	
    if(stp_btm == NULL)
    {
        return STP_BTM_OPERATION_FAIL;
    }
    else 
    {

#if 1 /* Paged dump */
        STP_BTM_INFO_FUNC("paged dump start++\n");
        retval = _stp_btm_dump_type(stp_btm,STP_OPID_BTM_PAGED_DUMP);
		if(retval)
		{
			STP_BTM_ERR_FUNC("paged dump fail\n");
		}

#else    
    	virtual_addr = wmt_plat_get_emi_ctrl_state_base_add(EXP_APMEM_CTRL_CHIP_SYNC_ADDR);
		if(!virtual_addr){
			STP_BTM_ERR_FUNC("get dump type virtual addr fail\n");
			return -1;
		}
		else
		{
    		dump_type = CONSYS_REG_READ(virtual_addr);
			STP_BTM_INFO_FUNC("dump type:%08x\n",dump_type);
		}

		if((dump_type & 0xfffff) == (CONSYS_PAGED_DUMP_START_ADDR & 0xfffff))
		{
			STP_BTM_INFO_FUNC("do paged dump\n");
        	retval = _stp_btm_dump_type(stp_btm,STP_OPID_BTM_PAGED_DUMP);
			if(retval)
			{
				STP_BTM_ERR_FUNC("paged dump fail,do full dump\n");
				_stp_btm_dump_type(stp_btm,STP_OPID_BTM_FULL_DUMP);
			}
		}
		else if((dump_type & 0xfffff) == (CONSYS_FULL_DUMP_START_ADDR & 0xfffff))
		{
			STP_BTM_INFO_FUNC("do full dump\n");
			retval = _stp_btm_dump_type(stp_btm,STP_OPID_BTM_FULL_DUMP);
		}
		else
		{
			STP_BTM_INFO_FUNC("do normal dump\n");
			retval = _stp_btm_dump_type(stp_btm,STP_OPID_BTM_DBG_DUMP);
		}
#endif        

    }
    return retval;
}

static inline INT32 _stp_notify_btm_poll_cpupcr(MTKSTP_BTM_T *stp_btm,UINT32 times,UINT32 sleep)
	{

    P_OSAL_OP     pOp;
    INT32         bRet;
    INT32 retval;

    if(stp_btm == NULL)
    {
        return STP_BTM_OPERATION_FAIL;
    }
    else 
    {
        pOp = _stp_btm_get_free_op(stp_btm);
        if (!pOp) 
        {
            //STP_BTM_WARN_FUNC("get_free_lxop fail \n");
            return -1;//break;
        }
        pOp->op.opId = STP_OPID_BTM_POLL_CPUPCR;
        pOp->signal.timeoutValue= 0;
		pOp->op.au4OpData[0] = times;
		pOp->op.au4OpData[1] = sleep;
        bRet = _stp_btm_put_act_op(stp_btm, pOp);
        STP_BTM_DBG_FUNC("OPID(%d) type(%d) bRet(%d) \n\n",
            pOp->op.opId,
            pOp->op.au4OpData[0],
            bRet);
        retval = (0 == bRet) ? STP_BTM_OPERATION_FAIL : STP_BTM_OPERATION_SUCCESS;
    }
    return retval;
}

static inline INT32 _stp_btm_notify_wmt_trace_wq(MTKSTP_BTM_T *stp_btm)
{
	P_OSAL_OP     pOp;
    INT32         bRet;
    INT32 retval;

    if(stp_btm == NULL)
    {
        return STP_BTM_OPERATION_FAIL;
    }
    else 
    {
        pOp = _stp_btm_get_free_op(stp_btm);
        if (!pOp) 
        {
            //STP_BTM_WARN_FUNC("get_free_lxop fail \n");
            return -1;//break;
        }
        pOp->op.opId = STP_OPID_BTM_PAGED_TRACE;
        pOp->signal.timeoutValue= 0;
        bRet = _stp_btm_put_act_op(stp_btm, pOp);
        STP_BTM_DBG_FUNC("OPID(%d) type(%d) bRet(%d) \n\n",
            pOp->op.opId,
            pOp->op.au4OpData[0],
            bRet);
        retval = (0 == bRet) ? STP_BTM_OPERATION_FAIL : STP_BTM_OPERATION_SUCCESS;
    }
    return retval;
}
static inline INT32 _stp_btm_do_fw_assert_via_emi(MTKSTP_BTM_T *stp_btm){

#if 0
    P_OSAL_OP       pOp;
    INT32           bRet;
    INT32 retval;

    if(stp_btm == NULL)
    {
        return STP_BTM_OPERATION_FAIL;
    }
    else 
    {
        pOp = _stp_btm_get_free_op(stp_btm);
        if (!pOp) 
        {
            STP_BTM_WARN_FUNC("get_free_lxop fail \n");
            return -1;//break;
        }
        pOp->op.opId = STP_OPID_BTM_FORCE_FW_ASSERT;
        pOp->signal.timeoutValue= 0;
        bRet = _stp_btm_put_act_op(stp_btm, pOp);
        STP_BTM_DBG_FUNC("OPID(%d) type(%d) bRet(%d) \n\n",
            pOp->op.opId,
            pOp->op.au4OpData[0],
            bRet);
        retval = (0 == bRet) ? STP_BTM_OPERATION_FAIL : STP_BTM_OPERATION_SUCCESS;
    }
    return retval;
#endif
	INT32 ret  = -1;

	ret = _stp_trigger_firmware_assert_via_emi();
	if(0 != _stp_get_dump_info(EXP_APMEM_CTRL_HOST_OUTBAND_ASSERT_W1))
	{
		STP_BTM_INFO_FUNC("EXP_APMEM_CTRL_HOST_OUTBAND_ASSERT_W1 has not clear,reset it\n");
		_stp_set_dump_info(EXP_APMEM_CTRL_HOST_OUTBAND_ASSERT_W1, 0x0);
	}
	else
	{
		STP_BTM_INFO_FUNC("EXP_APMEM_CTRL_HOST_OUTBAND_ASSERT_W1 reset by fw side\n");
	}

	return ret;
	
}
INT32 stp_btm_notify_wmt_rst_wq(MTKSTP_BTM_T *stp_btm)
{
    return _stp_btm_notify_wmt_rst_wq(stp_btm);
}

INT32 stp_btm_notify_stp_retry_wq(MTKSTP_BTM_T *stp_btm)
{
    return _stp_btm_notify_stp_retry_wq(stp_btm);
}

INT32 stp_btm_notify_coredump_timeout_wq(MTKSTP_BTM_T *stp_btm)
{
    return _stp_btm_notify_coredump_timeout_wq(stp_btm);
}

INT32 stp_btm_notify_wmt_dmp_wq(MTKSTP_BTM_T *stp_btm)
{
    return _stp_btm_notify_wmt_dmp_wq(stp_btm);
}

INT32 stp_btm_notify_wmt_trace_wq(MTKSTP_BTM_T *stp_btm)
{
	return _stp_btm_notify_wmt_trace_wq(stp_btm);
}

INT32 stp_notify_btm_poll_cpupcr(MTKSTP_BTM_T *stp_btm,UINT32 times, UINT32 sleep)
{
	return _stp_notify_btm_poll_cpupcr(stp_btm,times,sleep);
}
INT32 stp_notify_btm_poll_cpupcr_ctrl(UINT32 en)
{
	return stp_dbg_poll_cuppcr_ctrl(en);
}
INT32 stp_notify_btm_do_fw_assert_via_emi(MTKSTP_BTM_T *stp_btm)
{
	return _stp_btm_do_fw_assert_via_emi(stp_btm);
}
MTKSTP_BTM_T *stp_btm_init(void)
{
    INT32 i = 0x0;
    INT32 ret =-1;
    
    osal_unsleepable_lock_init(&stp_btm->wq_spinlock);
    osal_event_init(&stp_btm->STPd_event);
    stp_btm->wmt_notify = wmt_lib_btm_cb;
    
    RB_INIT(&stp_btm->rFreeOpQ, STP_BTM_OP_BUF_SIZE);
    RB_INIT(&stp_btm->rActiveOpQ, STP_BTM_OP_BUF_SIZE);

    /* Put all to free Q */
    for (i = 0; i < STP_BTM_OP_BUF_SIZE; i++) 
    {
         osal_signal_init(&(stp_btm->arQue[i].signal));
         _stp_btm_put_op(stp_btm, &stp_btm->rFreeOpQ, &(stp_btm->arQue[i]));
    }

    /*Generate PSM thread, to servie STP-CORE for packet retrying and core dump receiving*/
    stp_btm->BTMd.pThreadData = (VOID *)stp_btm;
    stp_btm->BTMd.pThreadFunc = (VOID *)_stp_btm_proc;
    osal_memcpy(stp_btm->BTMd.threadName, BTM_THREAD_NAME , osal_strlen(BTM_THREAD_NAME));

    ret = osal_thread_create(&stp_btm->BTMd);
    if (ret < 0) 
    {
        STP_BTM_ERR_FUNC("osal_thread_create fail...\n");
        goto ERR_EXIT1;
    }
    
    /* Start STPd thread*/
    ret = osal_thread_run(&stp_btm->BTMd);
    if(ret < 0)
    {
        STP_BTM_ERR_FUNC("osal_thread_run FAILS\n");
        goto ERR_EXIT1;
    }

    return stp_btm;

ERR_EXIT1:

    return NULL;

}

INT32 stp_btm_deinit(MTKSTP_BTM_T *stp_btm){

    UINT32 ret = -1;

    STP_BTM_INFO_FUNC("btm deinit\n");
    
    if(!stp_btm)
    {
        return STP_BTM_OPERATION_FAIL;
    }

    ret = osal_thread_destroy(&stp_btm->BTMd);
    if(ret < 0)
    {
        STP_BTM_ERR_FUNC("osal_thread_destroy FAILS\n");
        return STP_BTM_OPERATION_FAIL;
    }

    return STP_BTM_OPERATION_SUCCESS;
}


INT32 stp_btm_reset_btm_wq(MTKSTP_BTM_T *stp_btm)
{
    UINT32 i = 0;
    osal_lock_unsleepable_lock(&(stp_btm->wq_spinlock));
    RB_INIT(&stp_btm->rFreeOpQ, STP_BTM_OP_BUF_SIZE);
    RB_INIT(&stp_btm->rActiveOpQ, STP_BTM_OP_BUF_SIZE);
    osal_unlock_unsleepable_lock(&(stp_btm->wq_spinlock));
    /* Put all to free Q */
    for (i = 0; i < STP_BTM_OP_BUF_SIZE; i++) 
    {
         osal_signal_init(&(stp_btm->arQue[i].signal));
         _stp_btm_put_op(stp_btm, &stp_btm->rFreeOpQ, &(stp_btm->arQue[i]));
    }
    
    return 0;
}


