#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "e1000_dev.h"
#include "net.h"

#define TX_RING_SIZE 16
static struct tx_desc tx_ring[TX_RING_SIZE] __attribute__((aligned(16)));
static struct mbuf *tx_mbufs[TX_RING_SIZE];

#define RX_RING_SIZE 16
static struct rx_desc rx_ring[RX_RING_SIZE] __attribute__((aligned(16)));
static struct mbuf *rx_mbufs[RX_RING_SIZE];

// remember where the e1000's registers live.
static volatile uint32 *regs;

struct spinlock e1000_lock;

// called by pci_init().
// xregs is the memory address at which the
// e1000's registers are mapped.
void
e1000_init(uint32 *xregs)
{
  int i;

  //初始化锁 后续传输的时候 可以用锁保护
  initlock(&e1000_lock, "e1000");

  regs = xregs; 

  // Reset the device
  regs[E1000_IMS] = 0; // disable interrupts
  regs[E1000_CTL] |= E1000_CTL_RST;
  regs[E1000_IMS] = 0; // redisable interrupts
  __sync_synchronize();

  // [E1000 14.5] Transmit initialization
  memset(tx_ring, 0, sizeof(tx_ring));
  for (i = 0; i < TX_RING_SIZE; i++) {
    tx_ring[i].status = E1000_TXD_STAT_DD;
    tx_mbufs[i] = 0;
  }
  regs[E1000_TDBAL] = (uint64) tx_ring;
  if(sizeof(tx_ring) % 128 != 0)
    panic("e1000");
  regs[E1000_TDLEN] = sizeof(tx_ring);
  regs[E1000_TDH] = regs[E1000_TDT] = 0;
  
  // [E1000 14.4] Receive initialization
  memset(rx_ring, 0, sizeof(rx_ring));
  for (i = 0; i < RX_RING_SIZE; i++) {
    rx_mbufs[i] = mbufalloc(0);
    if (!rx_mbufs[i])
      panic("e1000");
    rx_ring[i].addr = (uint64) rx_mbufs[i]->head;
  }
  regs[E1000_RDBAL] = (uint64) rx_ring;
  if(sizeof(rx_ring) % 128 != 0)
    panic("e1000");
  regs[E1000_RDH] = 0;
  regs[E1000_RDT] = RX_RING_SIZE - 1;
  regs[E1000_RDLEN] = sizeof(rx_ring);

  // filter by qemu's MAC address, 52:54:00:12:34:56
  regs[E1000_RA] = 0x12005452;
  regs[E1000_RA+1] = 0x5634 | (1<<31);
  // multicast table
  for (int i = 0; i < 4096/32; i++)
    regs[E1000_MTA + i] = 0;

  // transmitter control bits.
  regs[E1000_TCTL] = E1000_TCTL_EN |  // enable
    E1000_TCTL_PSP |                  // pad short packets
    (0x10 << E1000_TCTL_CT_SHIFT) |   // collision stuff
    (0x40 << E1000_TCTL_COLD_SHIFT);
  regs[E1000_TIPG] = 10 | (8<<10) | (6<<20); // inter-pkt gap

  // receiver control bits.
  regs[E1000_RCTL] = E1000_RCTL_EN | // enable receiver
    E1000_RCTL_BAM |                 // enable broadcast
    E1000_RCTL_SZ_2048 |             // 2048-byte rx buffers
    E1000_RCTL_SECRC;                // strip CRC
  
  // ask e1000 for receive interrupts.
  regs[E1000_RDTR] = 0; // interrupt after every received packet (no timer)
  regs[E1000_RADV] = 0; // interrupt after every packet (no timer)
  regs[E1000_IMS] = (1 << 7); // RXDW -- Receiver Descriptor Write Back
}

int
e1000_transmit(struct mbuf *m)
{
  //
  // Your code here.
  //
  // the mbuf contains an ethernet frame; program it into
  // the TX descriptor ring so that the e1000 sends it. Stash
  // a pointer so that it can be freed after sending.
  //

  acquire(&e1000_lock);

  // printf("e1000_transmit has been called\n");

  // 查询ring里下一个packet的下标
  int idx = regs[E1000_TDT];

  if ((tx_ring[idx].status & E1000_TXD_STAT_DD) == 0) {
    // 之前的传输还没有完成
    release(&e1000_lock);
    return -1;
  }

  // 释放上一个包的内存
  if (tx_mbufs[idx])
    mbuffree(tx_mbufs[idx]);

  // 把这个新的网络包的pointer塞到ring这个下标位置
  tx_mbufs[idx] = m;
  tx_ring[idx].length = m->len;
  tx_ring[idx].addr = (uint64) m->head;
  tx_ring[idx].cmd = E1000_TXD_CMD_RS | E1000_TXD_CMD_EOP;

  //
  tx_ring[idx].status = 0;

  // 通过将一加到E1000_TDT再对TX_RING_SIZE取模来更新环位置
  regs[E1000_TDT] = (idx + 1) % TX_RING_SIZE;

  release(&e1000_lock);
  return 0;
}

static void
e1000_recv(void)
{
  //
  // Your code here.
  //
  // Check for packets that have arrived from the e1000
  // Create and deliver an mbuf for each packet (using net_rx()).
  //
  while (1)
  {
    int idx;

    //向E1000询问下一个等待接收数据包（如果有）所在的环索引
    idx = (regs[E1000_RDT] + 1) % RX_RING_SIZE;

    // 检查描述符status部分中的E1000_RXD_STAT_DD位来检查新数据包是否可用
    if ((rx_ring[idx].status & E1000_RXD_STAT_DD) == 0)
    {
      return;
    }

    // 将mbuf的m->len更新为描述符中报告的长度
    rx_mbufs[idx]->len = rx_ring[idx].length;

    // 使用net_rx()将mbuf传送到网络栈
    net_rx(rx_mbufs[idx]);

    // 使用mbufalloc()分配一个新的mbuf，以替换刚刚给net_rx()的mbuf
    rx_mbufs[idx] = mbufalloc(0);

    // 将描述符的状态位清除为零
    rx_ring[idx].status = 0;

    // 将其数据指针（m->head）编程到描述符中
    rx_ring[idx].addr = (uint64)rx_mbufs[idx]->head;

    // 将E1000_RDT寄存器更新为最后处理的环描述符的索引
    regs[E1000_RDT] = idx;
  }
}

void
e1000_intr(void)
{
  // tell the e1000 we've seen this interrupt;
  // without this the e1000 won't raise any
  // further interrupts.
  regs[E1000_ICR] = 0xffffffff;

  e1000_recv();
}
