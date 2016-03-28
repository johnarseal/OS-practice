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

// testnum is set in main.cc
int testnum = 3;

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
		
	break;
    default:
	printf("No test specified.\n");
	break;
    }
}

