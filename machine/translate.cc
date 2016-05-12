// translate.cc 
//	Routines to translate virtual addresses to physical addresses.
//	Software sets up a table of legal translations.  We look up
//	in the table on every memory reference to find the true physical
//	memory location.
//
// Two types of translation are supported here.
//
//	Linear page table -- the virtual page # is used as an index
//	into the table, to find the physical page #.
//
//	Translation lookaside buffer -- associative lookup in the table
//	to find an entry with the same virtual page #.  If found,
//	this entry is used for the translation.
//	If not, it traps to software with an exception. 
//
//	In practice, the TLB is much smaller than the amount of physical
//	memory (16 entries is common on a machine that has 1000's of
//	pages).  Thus, there must also be a backup translation scheme
//	(such as page tables), but the hardware doesn't need to know
//	anything at all about that.
//
//	Note that the contents of the TLB are specific to an address space.
//	If the address space changes, so does the contents of the TLB!
//
// DO NOT CHANGE -- part of the machine emulation
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "machine.h"
#include "addrspace.h"
#include "system.h"

// Routines for converting Words and Short Words to and from the
// simulated machine's format of little endian.  These end up
// being NOPs when the host machine is also little endian (DEC and Intel).

unsigned int
WordToHost(unsigned int word) {
#ifdef HOST_IS_BIG_ENDIAN
	 register unsigned long result;
	 result = (word >> 24) & 0x000000ff;
	 result |= (word >> 8) & 0x0000ff00;
	 result |= (word << 8) & 0x00ff0000;
	 result |= (word << 24) & 0xff000000;
	 return result;
#else 
	 return word;
#endif /* HOST_IS_BIG_ENDIAN */
}

unsigned short
ShortToHost(unsigned short shortword) {
#ifdef HOST_IS_BIG_ENDIAN
	 register unsigned short result;
	 result = (shortword << 8) & 0xff00;
	 result |= (shortword >> 8) & 0x00ff;
	 return result;
#else 
	 return shortword;
#endif /* HOST_IS_BIG_ENDIAN */
}

unsigned int
WordToMachine(unsigned int word) { return WordToHost(word); }

unsigned short
ShortToMachine(unsigned short shortword) { return ShortToHost(shortword); }


//----------------------------------------------------------------------
// Machine::ReadMem
//      Read "size" (1, 2, or 4) bytes of virtual memory at "addr" into 
//	the location pointed to by "value".
//
//   	Returns FALSE if the translation step from virtual to physical memory
//   	failed.
//
//	"addr" -- the virtual address to read from
//	"size" -- the number of bytes to read (1, 2, or 4)
//	"value" -- the place to write the result
//----------------------------------------------------------------------

bool
Machine::ReadMem(int addr, int size, int *value)
{
    int data;
    ExceptionType exception;
    int physicalAddress;
    
    DEBUG('a', "Reading VA 0x%x, size %d\n", addr, size);
    
    exception = Translate(addr, &physicalAddress, size, FALSE);
    if (exception != NoException) {
		machine->RaiseException(exception, addr);
		return FALSE;
    }
    switch (size) {
      case 1:
		data = machine->mainMemory[physicalAddress];
		*value = data;
	break;
	
      case 2:
		data = *(unsigned short *) &machine->mainMemory[physicalAddress];
		*value = ShortToHost(data);
	break;
	
      case 4:
		data = *(unsigned int *) &machine->mainMemory[physicalAddress];
		*value = WordToHost(data);
	break;

      default: ASSERT(FALSE);
    }
    
    DEBUG('a', "\tvalue read = %8.8x\n", *value);
    return (TRUE);
}

//----------------------------------------------------------------------
// Machine::WriteMem
//      Write "size" (1, 2, or 4) bytes of the contents of "value" into
//	virtual memory at location "addr".
//
//   	Returns FALSE if the translation step from virtual to physical memory
//   	failed.
//
//	"addr" -- the virtual address to write to
//	"size" -- the number of bytes to be written (1, 2, or 4)
//	"value" -- the data to be written
//----------------------------------------------------------------------

bool
Machine::WriteMem(int addr, int size, int value)
{
    ExceptionType exception;
    int physicalAddress;
     
    DEBUG('a', "Writing VA 0x%x, size %d, value 0x%x\n", addr, size, value);

    exception = Translate(addr, &physicalAddress, size, TRUE);
    if (exception != NoException) {
	machine->RaiseException(exception, addr);
	return FALSE;
    }
    switch (size) {
      case 1:
	machine->mainMemory[physicalAddress] = (unsigned char) (value & 0xff);
	break;

      case 2:
	*(unsigned short *) &machine->mainMemory[physicalAddress]
		= ShortToMachine((unsigned short) (value & 0xffff));
	break;
      
      case 4:
	*(unsigned int *) &machine->mainMemory[physicalAddress]
		= WordToMachine((unsigned int) value);
	break;
	
      default: ASSERT(FALSE);
    }
    
    return TRUE;
}

//----------------------------------------------------------------------
// Machine::Translate
// 	Translate a virtual address into a physical address, using 
//	either a page table or a TLB.  Check for alignment and all sorts 
//	of other errors, and if everything is ok, set the use/dirty bits in 
//	the translation table entry, and store the translated physical 
//	address in "physAddr".  If there was an error, returns the type
//	of the exception.
//
//	"virtAddr" -- the virtual address to translate
//	"physAddr" -- the place to store the physical address
//	"size" -- the amount of memory being read or written
// 	"writing" -- if TRUE, check the "read-only" bit in the TLB
//----------------------------------------------------------------------

ExceptionType
Machine::Translate(int virtAddr, int* physAddr, int size, bool writing)
{
    int i;
    unsigned int vpn, offset;
    TranslationEntry *entry;
    unsigned int pageFrame;

    DEBUG('a', "\tTranslate 0x%x, %s: ", virtAddr, writing ? "write" : "read");

// check for alignment errors
    if (((size == 4) && (virtAddr & 0x3)) || ((size == 2) && (virtAddr & 0x1))){
	DEBUG('a', "alignment problem at %d, size %d!\n", virtAddr, size);
	return AddressErrorException;
    }
    
    // we must have either a TLB or a page table, but not both!
	// zz: who made this stupid assertion???
//    ASSERT(tlb == NULL || pageTable == NULL);	
//    ASSERT(tlb != NULL || pageTable != NULL);	

// calculate the virtual page number, and offset within the page,
// from the virtual address
    vpn = (unsigned) virtAddr / PageSize;
    offset = (unsigned) virtAddr % PageSize;
    
    if (tlb == NULL) {		// => page table => vpn is index into table
		entry = pageTable->getPage(currentThread->threadId,vpn);
		if(entry == NULL){
			return PageFaultException;
		}
    } else {					// using tlb
        for (entry = NULL, i = 0; i < TLBSize; i++) {
    	    if (tlb->tlbTable[i].valid && (tlb->tlbTable[i].threadId == currentThread->threadId) && (tlb->tlbTable[i].virtualPage == vpn)) {
				entry = &(tlb->tlbTable[i]);			// FOUND!
				tlb->hitRecord[i]++;					// tlb hit!
				stats->tlbHit++;
				break;
			}
		}
		if (entry == NULL) {				// not found
			DEBUG('a', "*** no valid TLB entry found for this virtual page!\n");
			stats->tlbMiss++;
			return TLBMissException;		// really, this is a TLB fault,
							// the page may be in memory,
							// but not in the TLB
		}
    }

    if (entry->readOnly && writing) {	// trying to write to a read-only page
	DEBUG('a', "%d mapped read-only at %d in TLB!\n", virtAddr, i);
	return ReadOnlyException;
    }
    pageFrame = entry->physicalPage;

    // if the pageFrame is too big, there is something really wrong! 
    // An invalid translation was loaded into the page table or TLB. 
    if (pageFrame >= NumPhysPages) { 
	DEBUG('a', "*** frame %d > %d!\n", pageFrame, NumPhysPages);
	return BusErrorException;
    }
    entry->use = TRUE;		// set the use, dirty bits
    if (writing)
	entry->dirty = TRUE;
    *physAddr = pageFrame * PageSize + offset;
    ASSERT((*physAddr >= 0) && ((*physAddr + size) <= MemorySize));
    DEBUG('a', "phys addr = 0x%x\n", *physAddr);
    return NoException;
}


//	TLBbuffer implemented by zz
TLBuffer::TLBuffer(int bfSize){
	int i;
	tlbTable = new TranslationEntry[bfSize];
	hitRecord = new int[bfSize];
	bufferSize = bfSize;
	for (i = 0; i < bfSize; i++) {
		tlbTable[i].valid = FALSE;
		hitRecord[i] = 0;
	}
}

TLBuffer::~TLBuffer(){
	delete tlbTable;
	delete hitRecord;
}

// swap the TranslationEntry from pageTable to TLB when tlb miss
// implemented by zz
void
TLBuffer::Swap(){
	int missingVAddr,swapIndex,i,minHit;
	int vpn;
	TranslationEntry *entry;
	
	missingVAddr = machine->ReadRegister(BadVAddrReg);
	vpn =  missingVAddr / PageSize;
	
	// get the page from the page table
	entry = machine->pageTable->getPage(currentThread->threadId,vpn);
	if(entry == NULL){
		// if the page is not in the page table, raise a page fault, swap it from the disk
		machine->pageTable->Swap(vpn);
		entry = machine->pageTable->getPage(currentThread->threadId,vpn);
		// if it is still not in page table, swap failed, halt
		if(entry == NULL){
			ASSERT(FALSE);
		}
	}
	swapIndex = 0;
	minHit = hitRecord[0];
	for(i = 0; i < bufferSize; i++){
		if(!tlbTable[i].valid) {
			swapIndex = i;
			break;
		}
		if(hitRecord[i] < minHit) {
			minHit = hitRecord[i];
			swapIndex = i;
		}
	}
	tlbTable[swapIndex] = *(entry);
	hitRecord[swapIndex] = 1;
}


//	a page table implemented by zz
PageTable::PageTable(int bfSize){
	int i;
	entrySize = bfSize;
	pgTableEntry = new TranslationEntry[bfSize];
	hitRecord = new int[bfSize];
	for (i = 0; i < bfSize; i++) {
		pgTableEntry[i].valid = FALSE;
		pgTableEntry[i].use = FALSE;
		pgTableEntry[i].dirty = FALSE;
		pgTableEntry[i].readOnly = FALSE;
		pgTableEntry[i].physicalPage = i;
		hitRecord[i] = 0;
	}
}

PageTable::~PageTable(){
	delete pgTableEntry;
	delete hitRecord;
}

TranslationEntry *
PageTable::getPage(int threadId, int vpn){
	int i;
	for (i = 0; i < entrySize; i++) {
		if((pgTableEntry[i].threadId == threadId) && (pgTableEntry[i].virtualPage == vpn) && pgTableEntry[i].valid){
			hitRecord[i]++;
			break;
		}
	}
	if(i < entrySize){
		return &pgTableEntry[i];
	}
	else{
		return NULL;
	}
}

void
PageTable::Swap(int vpn){
	int codeBegin = currentThread->space->noffH.code.virtualAddr;
	int codeEnd = currentThread->space->noffH.code.virtualAddr + currentThread->space->noffH.code.size;
	
	int initDataBegin = currentThread->space->noffH.initData.virtualAddr;
	int initDataEnd = currentThread->space->noffH.initData.virtualAddr + currentThread->space->noffH.initData.size;
	
	int uninitDataBegin = currentThread->space->noffH.uninitData.virtualAddr;
	int uninitDataEnd = currentThread->space->noffH.uninitData.virtualAddr + currentThread->space->noffH.uninitData.size;
	
	int requestVA = vpn * PageSize;
	int size1,i;
	
	OpenFile *executable = fileSystem->Open(currentThread->userFileName);
	
	int swapIndex = 0;
	int minHit = hitRecord[0];
	
	stats->numPageFaults++;
	
	for(i = 0; i < entrySize; i++){
		if(!pgTableEntry[i].valid) {
			swapIndex = i;
			break;
		}
		if(hitRecord[i] < minHit) {
			minHit = hitRecord[i];
			swapIndex = i;
		}
	}
	
	//invalidate the tlb entry
	for(i = 0; i < machine->tlb->bufferSize; i++){
		if(machine->tlb->tlbTable[i].threadId == pgTableEntry[swapIndex].threadId && machine->tlb->tlbTable[i].virtualPage == pgTableEntry[swapIndex].virtualPage){
			machine->tlb->tlbTable[i].valid = FALSE;
		}
	}

	pgTableEntry[swapIndex].readOnly = FALSE;
	pgTableEntry[swapIndex].threadId = currentThread->threadId;
	pgTableEntry[swapIndex].virtualPage = vpn;
	pgTableEntry[swapIndex].valid = TRUE;
	hitRecord[swapIndex] = 1;
	
	if (requestVA >= codeBegin && requestVA < codeEnd){
		// if the request page cross a segment
		if (requestVA + PageSize > codeEnd){
			size1 = codeEnd - requestVA;
			executable->ReadAt(&(machine->mainMemory[swapIndex * PageSize]),
			size1, currentThread->space->noffH.code.inFileAddr + requestVA - codeBegin);
			if(currentThread->space->noffH.uninitData.size != 0){
				executable->ReadAt(&(machine->mainMemory[swapIndex * PageSize+size1]),
				PageSize - size1, currentThread->space->noffH.uninitData.inFileAddr);				
			}
		}
		else{	//else just read the code page
			executable->ReadAt(&(machine->mainMemory[swapIndex * PageSize]),PageSize, currentThread->space->noffH.code.inFileAddr + requestVA - codeBegin);
			pgTableEntry[swapIndex].readOnly = TRUE;
		}
	}
	else if(requestVA >= uninitDataBegin && requestVA < uninitDataEnd){
		executable->ReadAt(&(machine->mainMemory[swapIndex * PageSize]),
			PageSize, currentThread->space->noffH.uninitData.inFileAddr + requestVA - uninitDataBegin);		
	}
	else if(requestVA >= initDataBegin && requestVA < initDataEnd){
		executable->ReadAt(&(machine->mainMemory[swapIndex * PageSize]),
			PageSize, currentThread->space->noffH.initData.inFileAddr + requestVA - initDataBegin);		
	}
	delete executable;
}















