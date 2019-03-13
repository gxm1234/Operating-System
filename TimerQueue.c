/************************************************************************

 This code includes functions used for doing operations on timer queue.

 void InitialTimer();
 Used for initializing the timer queue.

 void InsertTimerTail(void *insert);
 Used for inserting an item to the tail of the timer queue.
 void *insert is the address of the item you want to insert.

 void InsertTimerHead(void *insert);
 Used for inserting an item to the head of the timer queue.
 void *insert is the address of the item you want to insert.

 void InsertTimerOrder(void *insert, int order);
 Used for inserting an item to the timer queue with an specific order.
 void *insert is the address of the item you want to insert.
 int order is the order you want to give to this item.

 void RemoveTimerItem(void *insert);
 Used for removing an item from the timer queue.
 void *insert is the address of the item you want to remove.

 void RemoveTimerHead();
 Used for removing the item at the head of the timer queue.

 int TimerExists(void *insert);
 Used for checking whether a specific item exists in the timer queue or not.
 void *insert is the address of the item you want to check.
 If exist - return 1
 If not exist - return 0

 int TimerEmpty();
 Used for checking whether timer queue is empty or not.
 If empty - return 1
 If not empty - return 0

 void TimerQPrint();
 Used for printing the whole structure of timer queue.
 A function used for debugging.

 int TimerReadPID(int order);
 Used for reading the pid of PCB item at a specific location of timer queue.
 int order is the order of item you wnat to find.
 (For example, first item has order 0, second item has order 1, and so on)
 return the corresponding PID, if doesn't exist, return -1.
 ************************************************************************/
#include             "global.h"
#include             "syscalls.h"
#include             "protos.h"
#include             "string.h"
#include             <stdlib.h>
#include             <ctype.h>
#include             <unistd.h>

#define                  DO_LOCK                     1
#define                  DO_UNLOCK                   0
#define                  SUSPEND_UNTIL_LOCKED        TRUE
#define                  DO_NOT_SUSPEND              FALSE
#define                  MEMORY_INTERLOCK_BASE       0x7FE00000


int TimerID;//Used for storing QID
INT32 LockResult;//Used for doing lock and unlock

//PCB structure
struct PCB {
	long Context;//Context of the process
	long pid;//Process ID of the process
	char *name;//Name of the process
	long order;//Priority of the process
	int ptime;//Wake-up time of the process
	int status;//Used for checking whether this process has been terminated or not
			   //(when status=0, it means that this process have been terminated)
	int diskid;//Used for storing disk id when doing some disk operations
};


// Used for initializing the timer queue.
void InitialTimer()
{
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 2, DO_LOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult);//Lock
	TimerID = QCreate("TimerQ");//Create a queue called "TimerQ"
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 2, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult);//Unlock
}

// Used for inserting an item to the tail of the timer queue.
//void *insert is the address of the item you want to insert.
void InsertTimerTail(void *insert)
{
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 2, DO_LOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult);
	QInsertOnTail(TimerID, insert);//Insert the item to the tail of timer queue
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 2, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult);
}

//Used for inserting an item to the head of the timer queue.
//void *insert is the address of the item you want to insert.
void InsertTimerHead(void *insert)
{
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 2, DO_LOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult);
	QInsert(TimerID,0,insert);//Insert the item to the timer queue with an order of 0
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 2, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult);
}

//Used for inserting an item to the timer queue with an specific order.
//void *insert is the address of the item you want to insert.
//int order is the order you want to give to this item.
void InsertTimerOrder(void *insert,int order)
{
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 2, DO_LOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult);
	QInsert(TimerID, order, insert);//Insert the item to the timer queue with an order
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 2, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult);
}

// Used for removing an item from the timer queue.
//void *insert is the address of the item you want to remove.
void RemoveTimerItem(void *insert)
{
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 2, DO_LOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult);
	QRemoveItem(TimerID, insert);//Remove the item from the timer queue
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 2, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult);
}

// Used for removing the item at the head of the timer queue.
void RemoveTimerHead()
{
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 2, DO_LOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult);
	QRemoveHead(TimerID);//Remove the head item from the timer queue
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 2, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult);
}

//Used for checking whether a specific item exists in the timer queue or not.
//void *insert is the address of the item you want to check.
//If exist - return 1
//If not exist - return 0
void *TimerExists(void *insert)
{
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 2, DO_LOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult);
	int *exist = (int *)QItemExists(TimerID, insert);//Check whether this item exists in timer queue or not
	if (exist == -1)
	{
		//Not exist
		READ_MODIFY(MEMORY_INTERLOCK_BASE + 2, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,
			&LockResult);
		return 0;
	}
	else
	{
		//Exist
		READ_MODIFY(MEMORY_INTERLOCK_BASE + 2, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,
			&LockResult);
		return 1;
	}
}


// Used for checking whether timer queue is empty or not.
//If empty - return 1
//If not empty - return 0
int TimerEmpty() {
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 2, DO_LOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult);
	int *isempty = (int *)QNextItemInfo(TimerID);//Check whether the timer queue is empty or not
	if (isempty == -1)
	{
		//Empty
		READ_MODIFY(MEMORY_INTERLOCK_BASE + 2, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,
			&LockResult);
		return 1;
	}
	else
	{
		//Not empty
		READ_MODIFY(MEMORY_INTERLOCK_BASE + 2, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,
			&LockResult);
		return 0;
	}
}

//Used for printing the whole structure of timer queue.
//A function used for debugging.
void TimerQPrint() {
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 2, DO_LOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult);
	QPrint(TimerID);//Do the print
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 2, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult);
}

//Used for reading the pid of PCB item at a specific location of timer queue.
//int order is the order of item you wnat to find.
//(For example, first item has order 0, second item has order 1, and so on)
//return the corresponding PID, if doesn't exist, return -1.
long TimerReadPID(int order) {
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 2, DO_LOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult);
	struct PCB *op;//Used for storing the return value
	int *check = (int *)QWalk(TimerID, order);//Check whether the order is valid or not
	if (check == -1)
	{
		//Not exist
		READ_MODIFY(MEMORY_INTERLOCK_BASE + 2, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,
			&LockResult);
		return -1;
	}
	else
	{
		//Exist
		op = QWalk(TimerID, order);//Get the specific item from timer queue
	}
	int pid = op->pid;//Get the pid of returning PCB
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 2, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult);
	return pid;//Return the pid

}



