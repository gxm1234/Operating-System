/************************************************************************

 This code includes functions used for doing operations on ready queue.

 void InitialReady();
 Used for initializing the ready queue.

 void InsertReadyTail(void *insert);
 Used for inserting an item to the tail of the ready queue.
 void *insert is the address of the item you want to insert.

 void InsertReadyHead(void *insert);
 Used for inserting an item to the head of the ready queue.
 void *insert is the address of the item you want to insert.

 void InsertReadyOrder(void *insert, int order);
 Used for inserting an item to the ready queue with an specific order.
 void *insert is the address of the item you want to insert.
 int order is the order you want to give to this item.

 void RemoveReadyItem(void *insert);
 Used for removing an item from the ready queue.
 void *insert is the address of the item you want to remove.

 void RemoveReadyHead();
 Used for removing the item at the head of the ready queue.

 int ReadyExists(void *insert);
 Used for checking whether a specific item exists in the ready queue or not.
 void *insert is the address of the item you want to check.
 If exist - return 1
 If not exist - return 0

 int ReadyEmpty();
 Used for checking whether ready queue is empty or not.
 If empty - return 1
 If not empty - return 0

 void ReadyQPrint();
 Used for printing the whole structure of ready queue.
 A function used for debugging.

 int ReadyReadPID(int order);
 Used for reading the pid of PCB item at a specific location of ready queue.
 int order is the order of item you wnat to find.
 (For example, first item has order 0, second item has order 1, and so on)
 return the corresponding PID, if doesn't exist, return -1.

 char *ReadyReadName(int order);
 Used for reading the name of PCB item at a specific location of ready queue.
 int order is the order of item you wnat to find.
 (For example, first item has order 0, second item has order 1, and so on)
 return the corresponding name, if doesn't exist, return -1.
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

int ReadyID; //Used for storing QID
INT32 LockResult; //Used for doing lock and unlock

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


// Used for initializing the ready queue.
void InitialReady()
{
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 1, DO_LOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult);//Lock
	ReadyID = QCreate("ReadyQ");//Create a queue called "ReadyQ"
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 1, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult);//Unlock
}

// Used for inserting an item to the tail of the ready queue.
//void *insert is the address of the item you want to insert.
void InsertReadyTail(void *insert)
{
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 1, DO_LOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult);
	QInsertOnTail(ReadyID, insert);//Insert the item to the tail of ready queue
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 1, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult);
}

//Used for inserting an item to the head of the ready queue.
//void *insert is the address of the item you want to insert.
void InsertReadyHead(void *insert)
{
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 1, DO_LOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult);
	QInsert(ReadyID,0,insert);//Insert the item to the ready queue with an order of 0
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 1, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult);
}

//Used for inserting an item to the ready queue with an specific order.
//void *insert is the address of the item you want to insert.
//int order is the order you want to give to this item.
void InsertReadyOrder(void *insert, int order)
{
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 1, DO_LOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult);
	QInsert(ReadyID, order, insert);//Insert the item to the ready queue with an order
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 1, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult);
}

// Used for removing an item from the ready queue.
//void *insert is the address of the item you want to remove.
void RemoveReadyItem(void *insert)
{
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 1, DO_LOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult);
	QRemoveItem(ReadyID, insert);//Remove the item from the ready queue
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 1, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult);
}

// Used for removing the item at the head of the ready queue.
void RemoveReadyHead()
{
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 1, DO_LOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult);
	QRemoveHead(ReadyID);//Remove the head item from the ready queue
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 1, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult);
}

//Used for checking whether a specific item exists in the ready queue or not.
//void *insert is the address of the item you want to check.
//If exist - return 1
//If not exist - return 0
int ReadyExists(void *insert) 
{
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 1, DO_LOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult);
	int *exist = (int *)QItemExists(ReadyID, insert);//Check whether this item exists in ready queue or not
	if (exist == -1)
	{
		//Not exist
		READ_MODIFY(MEMORY_INTERLOCK_BASE + 1, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,
			&LockResult);
		return 0;
	}
	else
	{
		//Exist
		READ_MODIFY(MEMORY_INTERLOCK_BASE + 1, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,
			&LockResult);
		return 1;
	}
}

// Used for checking whether ready queue is empty or not.
//If empty - return 1
//If not empty - return 0
int ReadyEmpty() {
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 1, DO_LOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult);
	int *isempty=(int *)QNextItemInfo(ReadyID);//Check whether the ready queue is empty or not
	if (isempty == -1)
	{
		//Empty
		READ_MODIFY(MEMORY_INTERLOCK_BASE + 1, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,
			&LockResult);
		return 1;
	}
	else
	{
		//Not empty
		READ_MODIFY(MEMORY_INTERLOCK_BASE + 1, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,
			&LockResult);
		return 0;
	}
}

//Used for printing the whole structure of ready queue.
//A function used for debugging.
void ReadyQPrint() {
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 1, DO_LOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult);
	QPrint(ReadyID);//Do the print
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 1, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult);
}


//Used for reading the pid of PCB item at a specific location of ready queue.
//int order is the order of item you wnat to find.
//(For example, first item has order 0, second item has order 1, and so on)
//return the corresponding PID, if doesn't exist, return -1.
int ReadyReadPID(int order) {
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 1, DO_LOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult);
	struct PCB *op;//Used for storing the return value
	int *check = (int *)QWalk(ReadyID, order);//Check whether the order is valid or not
	if (check==-1)
	{
		//Not exist
		READ_MODIFY(MEMORY_INTERLOCK_BASE + 1, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,
			&LockResult);
		return -1;
	}
	else
	{
		//Exist
		op = QWalk(ReadyID, order);//Get the specific item from ready queue
	}
	int pid = op->pid;//Get the pid of returning PCB
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 1, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult);
	return pid;//Return the pid
}

// Used for reading the name of PCB item at a specific location of ready queue.
//int order is the order of item you wnat to find.
//(For example, first item has order 0, second item has order 1, and so on)
//return the corresponding name, if doesn't exist, return -1.
char *ReadyReadName(int order) {
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 1, DO_LOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult);
	struct PCB *op;//Used for store the return value
	int *check = (int *)QWalk(ReadyID, order);//Check whether the order is valid or not
	if (check == -1)
	{
		//Not exist
		READ_MODIFY(MEMORY_INTERLOCK_BASE + 1, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,
			&LockResult);
		return -1;
	}
	else
	{
		//Exist
		op = QWalk(ReadyID, order);//Get the specific item from ready queue
	}
	char *name = op->name;//Get the name of PCB
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 1, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult);
	return name;//Return the name
}