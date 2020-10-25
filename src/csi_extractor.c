/***************************************************************************
 *                                                                         *
 *          ###########   ###########   ##########    ##########           *
 *         ############  ############  ############  ############          *
 *         ##            ##            ##   ##   ##  ##        ##          *
 *         ##            ##            ##   ##   ##  ##        ##          *
 *         ###########   ####  ######  ##   ##   ##  ##    ######          *
 *          ###########  ####  #       ##   ##   ##  ##    #    #          *
 *                   ##  ##    ######  ##   ##   ##  ##    #    #          *
 *                   ##  ##    #       ##   ##   ##  ##    #    #          *
 *         ############  ##### ######  ##   ##   ##  ##### ######          *
 *         ###########    ###########  ##   ##   ##   ##########           *
 *                                                                         *
 *            S E C U R E   M O B I L E   N E T W O R K I N G              *
 *                                                                         *
 * Copyright (c) 2019 Matthias Schulz                                      *
 *                                                                         *
 * Permission is hereby granted, free of charge, to any person obtaining a *
 * copy of this software and associated documentation files (the           *
 * "Software"), to deal in the Software without restriction, including     *
 * without limitation the rights to use, copy, modify, merge, publish,     *
 * distribute, sublicense, and/or sell copies of the Software, and to      *
 * permit persons to whom the Software is furnished to do so, subject to   *
 * the following conditions:                                               *
 *                                                                         *
 * 1. The above copyright notice and this permission notice shall be       *
 *    include in all copies or substantial portions of the Software.       *
 *                                                                         *
 * 2. Any use of the Software which results in an academic publication or  *
 *    other publication which includes a bibliography must include         *
 *    citations to the nexmon project a) and the paper cited under b):     *
 *                                                                         *
 *    a) "Matthias Schulz, Daniel Wegemer and Matthias Hollick. Nexmon:    *
 *        The C-based Firmware Patching Framework. https://nexmon.org"     *
 *                                                                         *
 *    b) "Francesco Gringoli, Matthias Schulz, Jakob Link, and Matthias    *
 *        Hollick. Free Your CSI: A Channel State Information Extraction   *
 *        Platform For Modern Wi-Fi Chipsets. Accepted to appear in        *
 *        Proceedings of the 13th Workshop on Wireless Network Testbeds,   *
 *        Experimental evaluation & CHaracterization (WiNTECH 2019),       *
 *        October 2019."                                                   *
 *                                                                         *
 * 3. The Software is not used by, in cooperation with, or on behalf of    *
 *    any armed forces, intelligence agencies, reconnaissance agencies,    *
 *    defense agencies, offense agencies or any supplier, contractor, or   *
 *    research associated.                                                 *
 *                                                                         *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS *
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF              *
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  *
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY    *
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,    *
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE       *
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                  *
 *                                                                         *
 **************************************************************************/

#pragma NEXMON targetregion "patch"

#include <firmware_version.h>
#include <wrapper.h>
#include <structs.h>
#include <patcher.h>
#include <helper.h>
#include <capabilities.h>
#include <channels.h>

extern void prepend_ethernet_ipv4_udp_header(struct sk_buff *p);

#define WL_RSSI_ANT_MAX     4           /* max possible rx antennas */

// header of csi frame coming from ucode
struct d11csihdr {
    uint16 RxFrameSize;                 /* 0x000 Set to 0x2 for CSI frames */
    uint16 NexmonExt;                   /* 0x002 */
    uint16 NexmonCSICfg;		/* 0x004 Configuration of this CSI */
    uint16 NexmonCSILen;		/* 0x006 Number of bytes in this chunk */
    uint32 csi[];                       /* 0x008 Array of CSI data */
} __attribute__((packed));

// original hardware header
struct d11rxhdr {
    uint16 RxFrameSize;                 /* 0x000 Set to 0x2 for CSI frames */
    uint16 NexmonExt;                   /* 0x002 */
    uint16 PhyRxStatus_0;               /* 0x004 PhyRxStatus 15:0 */
    uint16 PhyRxStatus_1;               /* 0x006 PhyRxStatus 31:16 */
    uint16 PhyRxStatus_2;               /* 0x008 PhyRxStatus 47:32 */
    uint16 PhyRxStatus_3;               /* 0x00a PhyRxStatus 63:48 */
    uint16 PhyRxStatus_4;               /* 0x00c PhyRxStatus 79:64 */
    uint16 PhyRxStatus_5;               /* 0x00e PhyRxStatus 95:80 */
    uint16 RxStatus1;                   /* 0x010 MAC Rx Status */
    uint16 RxStatus2;                   /* 0x012 extended MAC Rx status */
    uint16 RxTSFTime;                   /* 0x014 RxTSFTime time of first MAC symbol + M_PHY_PLCPRX_DLY */
    uint16 RxChan;                      /* 0x016 gain code, channel radio code, and phy type */
} __attribute__((packed));

// header or regular frame coming from ucode
struct nexmon_d11rxhdr {
    struct d11rxhdr rxhdr;              /* 0x000 d11rxhdr */
    uint8 SrcMac[6];                    /* 0x018 source mac address */
} __attribute__((packed));

// header after process_frame_hook
struct wlc_d11rxhdr {
    struct d11rxhdr rxhdr;              /* 0x000 d11rxhdr */
    uint32  tsf_l;                      /* 0x018 TSF_L reading */
    int8    rssi;                       /* 0x01c computed instantaneous rssi in BMAC */
    int8    rxpwr0;                     /* 0x01d obsoleted, place holder for legacy ROM code. use rxpwr[] */
    int8    rxpwr1;                     /* 0x01e obsoleted, place holder for legacy ROM code. use rxpwr[] */
    int8    do_rssi_ma;                 /* 0x01f do per-pkt sampling for per-antenna ma in HIGH */
    int8    rxpwr[WL_RSSI_ANT_MAX];     /* 0x020 rssi for supported antennas */
    int8    rssi_qdb;                   /* 0x024 qdB portion of the computed rssi */
    uint8   PAD[2];                     /* 0x025 extra padding to fill up RXE_RXHDR_EXTRA */
} __attribute__((packed));

struct csi_udp_frame {
    struct ethernet_ip_udp_header hdrs;
    uint32 kk1;
    uint8 mac_1[6];
    uint8 mac_2[6];
    uint8 mac_3[6];
    uint16 fc;
    uint16 sc;
    int8 rssi;
    uint16 res_1;
    uint16 res_2;
    uint16 res_3;
    uint16 res_4;
    uint16 res_5;
    uint16 res_6;
    uint16 res_7;
    uint16 res_8;
    uint16 res_9;
    uint16 csiconf;
    uint16 chanspec;
    uint16 chip;
    uint32 csi_values[];
} __attribute__((packed));

struct int14 {signed int val:14;} __attribute__((packed));

uint16 missing_csi_frames = 0;
uint16 inserted_csi_values = 0;
struct sk_buff *p_csi = 0;
int8 last_rssi = 0;

void
create_new_csi_frame(struct wl_info *wl, uint16 csiconf, int length)
{
    struct osl_info *osh = wl->wlc->osh;
    // create new csi udp frame
    p_csi = pkt_buf_get_skb(osh, sizeof(struct csi_udp_frame) + length);
    if (p_csi == 0) {
        return;
    }
    // fill header
    struct csi_udp_frame *udpfrm = (struct csi_udp_frame *) p_csi->data;
    // add magic bytes, csi config and chanspec to new udp frame
    udpfrm->kk1 = 0x11111111;
    udpfrm->fc = 0;
    udpfrm->sc = 0;
    udpfrm->rssi = last_rssi;
    udpfrm->res_1 = 0xff;
    udpfrm->res_2 = 0xff;
    udpfrm->res_3 = 0xff;
    udpfrm->res_4 = 0xff;
    udpfrm->res_5 = 0xff;
    udpfrm->res_6 = 0xff;
    udpfrm->res_7 = 0xff;
    udpfrm->res_8 = 0xff;
    udpfrm->res_9 = 0xff;
    udpfrm->csiconf = csiconf;
    udpfrm->chanspec = get_chanspec(wl->wlc);
    udpfrm->chip = NEXMON_CHIP;
}

void
process_frame_hook(struct sk_buff *p, struct wlc_d11rxhdr *wlc_rxhdr, struct wlc_hw_info *wlc_hw, int tsf_l)
{
    struct osl_info *osh = wlc_hw->wlc->osh;
    struct wl_info *wl = wlc_hw->wlc->wl;

    if (wlc_rxhdr->rxhdr.RxFrameSize == 2) {
        struct d11csihdr *ucodecsifrm = (struct d11csihdr *) &(wlc_rxhdr->rxhdr);
        int missing = ucodecsifrm->NexmonCSICfg & 0xff;
        int tones = ucodecsifrm->NexmonCSILen;
        uint16 csiconf = (ucodecsifrm->NexmonCSICfg >> 8)&0x3f;
#define CSIDATA_PER_CHUNK   56
#define NEWCSI	0x4000
        // check this is a new frame
        if (ucodecsifrm->NexmonCSICfg & NEWCSI) {
            if (p_csi != 0) {
                printf("unexpected new csi, clearing old\n");
                pkt_buf_free_skb(osh, p_csi, 0);
            }
            create_new_csi_frame(wl, csiconf, missing * CSIDATA_PER_CHUNK);
            if (p_csi == 0) {
                printf("unable to allocate csi frame\n");
                pkt_buf_free_skb(osh, p, 0);
                return;
            }
            missing_csi_frames = missing;
            inserted_csi_values = 0;
        }
        else if (p_csi == 0) {
            printf("unexpected csi data\n");
            pkt_buf_free_skb(osh, p, 0);
            return;
        }
        else if (missing != missing_csi_frames) {
            printf("number of missing frames mismatch\n");
            pkt_buf_free_skb(osh, p, 0);
            pkt_buf_free_skb(osh, p_csi, 0);
            p_csi = 0;
            return;
        }

        struct csi_udp_frame *udpfrm = (struct csi_udp_frame *) p_csi->data;

        int i;
        for (i = 0; i < tones; i ++) {
            // csi format is 4bit null, int14 real, int14 imag
            // convert to int16 real, int16 imag
            struct int14 sint14;
            sint14.val = (ucodecsifrm->csi[i] >> 14) & 0x3fff;
            udpfrm->csi_values[inserted_csi_values] = (uint32)((int16)(sint14.val)) & 0xffff;
            sint14.val = ucodecsifrm->csi[i] & 0x3fff;
            udpfrm->csi_values[inserted_csi_values] |= ((uint32)((int16)(sint14.val))) << 16;
            inserted_csi_values++;
        }

        missing_csi_frames --;

        // send csi udp to host
        if (missing_csi_frames == 0) {

            // Last CSI frame contains more data we added 
            // in the read_csi section.
            memcpy(udpfrm->mac_1, &(ucodecsifrm->csi[tones]) + (sizeof(udpfrm->mac_1) * 0), sizeof(udpfrm->mac_1));
            memcpy(udpfrm->mac_2, &(ucodecsifrm->csi[tones]) + (sizeof(udpfrm->mac_1) * 1), sizeof(udpfrm->mac_1));
            memcpy(udpfrm->mac_3, &(ucodecsifrm->csi[tones]) + (sizeof(udpfrm->mac_1) * 2), sizeof(udpfrm->mac_1));

            udpfrm->fc = *(
                            (uint16*)(&(ucodecsifrm->csi[tones])) +
                            ((sizeof(udpfrm->mac_1)>>1) * 3) +
                            0
                        );

            udpfrm->sc = *(
                            (uint16*)(&(ucodecsifrm->csi[tones])) +
                            ((sizeof(udpfrm->mac_1)>>1) * 3) +
                            1
                        );
                
            udpfrm->res_1 = *(
                            (uint16*)(&(ucodecsifrm->csi[tones])) +
                            ((sizeof(udpfrm->mac_1)>>1) * 3) +
                            2
                        );

            udpfrm->res_2 = *(
                            (uint16*)(&(ucodecsifrm->csi[tones])) +
                            ((sizeof(udpfrm->mac_1)>>1) * 3) +
                            3
                        );

            udpfrm->res_3 = *(
                            (uint16*)(&(ucodecsifrm->csi[tones])) +
                            ((sizeof(udpfrm->mac_1)>>1) * 3) +
                            4
                        );

            udpfrm->res_4 = *(
                            (uint16*)(&(ucodecsifrm->csi[tones])) +
                            ((sizeof(udpfrm->mac_1)>>1) * 3) +
                            5
                        );

            udpfrm->res_5 = *(
                            (uint16*)(&(ucodecsifrm->csi[tones])) +
                            ((sizeof(udpfrm->mac_1)>>1) * 3) +
                            6
                        );

            udpfrm->res_6 = *(
                            (uint16*)(&(ucodecsifrm->csi[tones])) +
                            ((sizeof(udpfrm->mac_1)>>1) * 3) +
                            7
                        ); 

            udpfrm->res_7 = *(
                            (uint16*)(&(ucodecsifrm->csi[tones])) +
                            ((sizeof(udpfrm->mac_1)>>1) * 3) +
                            8
                        );

            udpfrm->res_8 = *(
                            (uint16*)(&(ucodecsifrm->csi[tones])) +
                            ((sizeof(udpfrm->mac_1)>>1) * 3) +
                            9
                        );

            udpfrm->res_9 = *(
                            (uint16*)(&(ucodecsifrm->csi[tones])) +
                            ((sizeof(udpfrm->mac_1)>>1) * 3) +
                            10
                        );                                                                       

            p_csi->len = sizeof(struct csi_udp_frame) + inserted_csi_values * sizeof(uint32);
            skb_pull(p_csi, sizeof(struct ethernet_ip_udp_header));
            prepend_ethernet_ipv4_udp_header(p_csi);
            wl->dev->chained->funcs->xmit(wl->dev, wl->dev->chained, p_csi);
            p_csi = 0;
        }
        pkt_buf_free_skb(osh, p, 0);
        return;
    }

    wlc_rxhdr->tsf_l = tsf_l;
    wlc_phy_rssi_compute(wlc_hw->band->pi, wlc_rxhdr);
    last_rssi = wlc_rxhdr->rssi;
    wlc_recv(wlc_hw->wlc, p);
}

__attribute__((at(0x1AAFCC, "", CHIP_VER_BCM4339, FW_VER_6_37_32_RC23_34_43_r639704)))
__attribute__((naked))
void
process_frame_prehook_off0xC(void)
{
    asm(
        "mov r2, r4\n"              // 2 bytes: move wlc_hw pointer to r2
        "ldr r0, [sp,0xC]\n"        // 2 bytes: load reference to p into r0
        "bl process_frame_hook\n"   // 4 bytes
        "nop\n"                     // 2 bytes: to overwrite existing instruction
        "nop\n"                     // 2 bytes: to overwrite existing instruction
        "nop\n"                     // 2 bytes: to overwrite existing instruction
        "nop\n"                     // 2 bytes: to overwrite existing instruction
        "nop\n"                     // 2 bytes: to overwrite existing instruction
        "nop\n"                     // 2 bytes: to overwrite existing instruction
    );
}
__attribute__((at(0x1B2012, "", CHIP_VER_BCM4358, FW_VER_7_112_300_14)))
__attribute__((at(0x1C1C3E, "", CHIP_VER_BCM43455c0, FW_VER_7_45_189)))
__attribute__((at(0x2456E8, "", CHIP_VER_BCM4366c0, FW_VER_10_10_122_20)))
__attribute__((naked))
void
process_frame_prehook_off0x8(void)
{
    asm(
        "mov r2, r4\n"              // 2 bytes: move wlc_hw pointer to r2
        "ldr r0, [sp,0x8]\n"        // 2 bytes: load reference to p into r0
        "bl process_frame_hook\n"   // 4 bytes
        "nop\n"                     // 2 bytes: to overwrite existing instruction
        "nop\n"                     // 2 bytes: to overwrite existing instruction
        "nop\n"                     // 2 bytes: to overwrite existing instruction
        "nop\n"                     // 2 bytes: to overwrite existing instruction
        "nop\n"                     // 2 bytes: to overwrite existing instruction
        "nop\n"                     // 2 bytes: to overwrite existing instruction
    );
}


// Increase d11rxhdr size in initvals (found: below 0c 04 02 00)
__attribute__((at(0x1D4370, "", CHIP_VER_BCM4339, FW_VER_6_37_32_RC23_34_43_r639704)))
__attribute__((at(0x1F5768, "", CHIP_VER_BCM43455c0, FW_VER_7_45_189)))
__attribute__((at(0x1DF03C, "", CHIP_VER_BCM4358, FW_VER_7_112_300_14)))
__attribute__((at(0x28395C, "", CHIP_VER_BCM4366c0, FW_VER_10_10_122_20)))
GenericPatch1(initvals_rxhdr_len0, (RXE_RXHDR_LEN * 2));

__attribute__((at(0x1D4388, "", CHIP_VER_BCM4339, FW_VER_6_37_32_RC23_34_43_r639704)))
__attribute__((at(0x1F5778, "", CHIP_VER_BCM43455c0, FW_VER_7_45_189)))
__attribute__((at(0x1DF054, "", CHIP_VER_BCM4358, FW_VER_7_112_300_14)))
__attribute__((at(0x28396C, "", CHIP_VER_BCM4366c0, FW_VER_10_10_122_20)))
GenericPatch1(initvals_rxhdr_len1, (RXE_RXHDR_LEN * 2));

// Increase d11rxhdr size in wlc->hwrxhdr variable in wlc_bmac_attach
__attribute__((at(0x1F580C, "", CHIP_VER_BCM4339, FW_VER_6_37_32_RC23_34_43_r639704)))
__attribute__((at(0x210F56, "", CHIP_VER_BCM43455c0, FW_VER_7_45_189)))
__attribute__((at(0x1F67F8, "", CHIP_VER_BCM4358, FW_VER_7_112_300_14)))
__attribute__((at(0x2F4328, "", CHIP_VER_BCM4366c0, FW_VER_10_10_122_20)))
GenericPatch1(hwrxoff, (RXE_RXHDR_LEN * 2) + RXE_RXHDR_EXTRA);

// Increase d11rxhdr size in wlc->hwrxoff_pktget variable in wlc_bmac_attach
__attribute__((at(0x1F5812, "", CHIP_VER_BCM4339, FW_VER_6_37_32_RC23_34_43_r639704)))
__attribute__((at(0x210F60, "", CHIP_VER_BCM43455c0, FW_VER_7_45_189)))
__attribute__((at(0x1F6802, "", CHIP_VER_BCM4358, FW_VER_7_112_300_14)))
__attribute__((at(0x2F4332, "", CHIP_VER_BCM4366c0, FW_VER_10_10_122_20)))
GenericPatch1(hwrxoff_pktget, (RXE_RXHDR_LEN * 2) + RXE_RXHDR_EXTRA + 2);

//Workaround to skip AMSDU frames. https://github.com/seemoo-lab/nexmon/issues/280#issuecomment-516731866
__attribute__((at(0x1B6B02, "", CHIP_VER_BCM43455c0,FW_VER_7_45_189)))
BPatch(wlc_monitor_amsdu_patch, 0x1B6B1E);