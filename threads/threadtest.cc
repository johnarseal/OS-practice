// threadtest.cc 
//	Simple test case for the threads assignment.
//
//	Create two threads, and have them context switch
//	back and forth between themselves by calling Thread::Yield, 
//	to illustratethe inner workings of the thread system.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "synch.h"

// testnum is set in main.cc
int testnum = 4;


//the consumer and producer model for synchronize test.
class Con_Pro{
	public:
		int count, maxSlot;
		bool* pool;
		Lock* poolLock;
		Condition* not_empty;
		Condition* not_full;
		
		//initialize a Consumer_Produce
		Con_Pro(int slot=2)
		{
			count = 0;
			maxSlot=slot;
			pool = new bool[slot];
			for (int i = 0; i < slot; i++)
			{
				pool[i] = 0;
			}
			poolLock = new Lock("poolLock");
			not_empty = new Condition("not_empty");
			not_full = new Condition("not_full");
		}
		~Con_Pro(){
			delete pool;
			delete poolLock;
			delete not_empty;
			delete not_full;
		}
		
};

Con_Pro conPro = Con_Pro();
//----------------------------------------------------------------------
// SimpleThread
// 	Loop 5 times, yielding the CPU to another ready thread 
//	each iteration.
//
//	"which" is simply a number identifying the thread, for debugging
//	purposes.
//----------------------------------------------------------------------

void
SimpleThread(int which)
{
    int num;
    
    for (num = 0; num < 5; num++) {
		printf("*** thread %d looped %d times\n", which, num);
		Thread::ListAllThreads();
        currentThread->Yield();
    }
}

void
SimpleThread2(int test2num)
{
	if(test2num == 0)
	{
		return;
	}
	Thread *t = new Thread("forked thread",test2num-1,0);
	t->Fork(SimpleThread2, test2num-1);
	printf("*** welcome to thread %d, its priority is %d\n", currentThread->getThreadId(), currentThread->getPriority());
	
}

void
SimpleThread3(int turn)
{
	for(int i = 0; i < turn; i++)
	{
		interrupt->OneTick();
	}
}

void
consumer(int id)
{
	conPro.poolLock->Acquire();
	while(conPro.count == 0)
	{
		printf("consumer %d is blocked due to empty pool\n",id);
		conPro.not_empty->Wait(conPro.poolLock);
	}
	int i;
	for(i = 0; i < conPro.maxSlot; i++)
	{
		if (conPro.pool[i] == 1)
		{
			conPro.pool[i] = 0;
			conPro.not_full->Signal(conPro.poolLock);
			break;
		}
	}
	printf("consumer %d consumed slot %d\n",id,i);
	conPro.count--;
	conPro.poolLock->Release();
}
void
producer(int id)
{
	conPro.poolLock->Acquire();
	while(conPro.count == conPro.maxSlot)
	{
		printf("producer %d is blocked due to full pool\n",id);
		conPro.not_full->Wait(conPro.poolLock);
	}
	int i;
	for(i = 0; i < conPro.maxSlot; i++)
	{
		if (conPro.pool[i] == 0)
		{
			conPro.pool[i] = 1;
			conPro.not_empty->Signal(conPro.poolLock);
			break;
		}
	}
	printf("producer %d produced in slot %d\n",id,i);
	conPro.count++;
	conPro.poolLock->Release();
}


//----------------------------------------------------------------------
// ThreadTest1
// 	Set up a ping-pong between two threads, by forking a thread 
//	to call SimpleThread, and then calling SimpleThread ourselves.
//----------------------------------------------------------------------

void
ThreadTest1()
{
    DEBUG('t', "Entering ThreadTest1");

    Thread *t = new Thread("forked thread");

    t->Fork(SimpleThread, 1);
    SimpleThread(0);
	
}

void
ThreadTest2()
{
    DEBUG('t', "Entering ThreadTest2");

    Thread *t = new Thread("forked thread",5,0);
	t->Fork(SimpleThread2, 5);
	
}

void
ThreadTest3()
{
    DEBUG('t', "Entering ThreadTest3");

	for(int i = 0; i < 5; i++)
	{
		Thread *t = new Thread("forked thread",i);
		t->Fork(SimpleThread3,(5-i)*20);
	}
	
}

void
ThreadTest4()
{
    DEBUG('t', "Entering ThreadTest4");
	int i;
	Thread* t;
	for(i = 0; i < 6; i++)
	{
		t = new Thread("consumer");
		t->Fork(consumer,i);		
	}
	for(i = 0; i < 6; i++)
	{
		t = new Thread("producer");
		t->Fork(producer,i);	
	}
}

//----------------------------------------------------------------------
// ThreadTest
// 	Invoke a test routine.
//----------------------------------------------------------------------

void
ThreadTest()
{
    switch (testnum) {
    case 1:
		ThreadTest1();break;
	case 2:
		ThreadTest2();break;
	case 3:
		ThreadTest3();break;
	case 4:
		ThreadTest4();break;
	break;
    default:
	printf("No test specified.\n");
	break;
    }
}

