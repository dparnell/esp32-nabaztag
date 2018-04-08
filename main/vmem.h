// VLISP Virtual Machine - 2006 - by Sylvain Huet
// Lowcost IS Powerfull

#ifndef _MEMORY_
#define _MEMORY_

#include "esp_task_wdt.h"

// #define VSIMU /// d�fini dans le Makefile : -DVSIMU
//#define VREAL
#define USE_SPI_SRAM
// taille en mots 32 bits (128kwords -> 512ko)
#ifdef USE_SPI_SRAM
#define VMEM_LENGTH	(1024*200)
#define vmem_heap ((int*)(0x3F800000))
#else
#define VMEM_LENGTH	(1024*32)
extern int vmem_heap[VMEM_LENGTH];
#endif
#define VMEM_GCTHRESHOLD 16

#define ENDLINE "\n"
#define uchar unsigned char
#define CLR_WDT esp_task_wdt_reset()

void dump(unsigned char *src,int len);

extern int vmem_heapindex;
extern int *vmem_top;
extern int vmem_stack;
extern int vmem_start;
extern int vmem_broken;

int vmemAllocBin(int size,int ext);
int vmemAllocTab(int size,int ext);
int vmemAllocTabClear(int size,int ext);
int vmemAllocString(char *p,int len);
int vmemPush(int val);
void vmemStacktotab(int n);

void vmemInit(int start);
void vmemSetstart(int start);

void vmemGC();


#define TYPE_BINARY 0	// type binaire pour allocation
#define TYPE_TAB 1	// type table pour allocation

#define PNTTOVAL(p) (1+((p)<<1))	// conversion pointeur vers mot mv
#define INTTOVAL(i) ((i)<<1)	// conversion entier vers mot mv

#define VALTOPNT(v) ((v)>>1)	// conversion mot mv vers pointeur
#define VALTOINT(v) ((v)>>1)	// conversion mot mv vers entier

#define NIL (-1)	// nil
#define ISVALPNT(v) ((v)&1)	// teste si un mot mv est un pointeur

#define HEADER_LENGTH 3
#define HEADER_SIZETYPE 0	// la taille contient le header
#define HEADER_GC 1	// bit 0 : marquage, autres bits : adresse apr�s GC
#define HEADER_LIST 2	// pile GC

#define HEADER_TYPE(p) (vmem_heap[p]&1)
#define HEADER_EXT(p) ((vmem_heap[p]>>1)&127)
#define HEADER_SIZE(p) (vmem_heap[p]>>8)
#define HEADER_USED(p) (vmem_heap[(p)+HEADER_GC]&1)
#define HEADER_MARK(p) vmem_heap[(p)+HEADER_GC]|=1

// Stack management
#define VPUSH(v) vmemPush(v)
#define VPULL() (vmem_top[vmem_stack++])
#define VDROP() (vmem_stack++)
#define VSTACKGET(n) (vmem_top[vmem_stack+(n)])
#define VSTACKSET(n,v) vmem_top[vmem_stack+(n)]=(v)
#define VDROPN(n) vmem_stack+=(n)
#define VCALLSTACKGET(off,n) (vmem_top[(off)-(n)])
#define VCALLSTACKSET(off,n,v) vmem_top[(off)-(n)]=(v)

// Heap management
#define VMALLOC(n) vmemAllocTab(n,0)
#define VMALLOCCLEAR(n) vmemAllocTabClear(n,0)
#define VMALLOCBIN(n) vmemAllocBin(n,0)
#define VMALLOCSTR(p,n) vmemAllocString(p,n)
#define VMKTAB(n) vmemStacktotab(n)
#define VSIZE(p) (((vmem_heap[p]>>8)+4)>>2)
#define VSIZEBIN(p) (vmem_heap[p]>>8)
#define VSTART(p) (&vmem_heap[(p)+HEADER_LENGTH])
#define VSTARTBIN(p) ((char*)&vmem_heap[(p)+HEADER_LENGTH])
#define VFETCH(p,i) (vmem_heap[(p)+HEADER_LENGTH+(i)])
#define VSTORE(p,i,v) vmem_heap[(p)+HEADER_LENGTH+(i)]=(v)



void vmemDumpHeap();
void vmemDumpStack();
void vmemDump();
void vmemDumpShort();

#endif
