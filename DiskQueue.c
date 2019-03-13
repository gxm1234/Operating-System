/************************************************************************

 This code includes functions used for doing operations on disk queue.

 void InitialDisk();
 Used for initializing the disk queue.

 void InsertDiskTail(void *insert);
 Used for inserting an item to the tail of the disk queue.
 void *insert is the address of the item you want to insert.

 void InsertDiskHead(void *insert);
 Used for inserting an item to the head of the disk queue.
 void *insert is the address of the item you want to insert.

 void InsertDiskOrder(void *insert, int order);
 Used for inserting an item to the disk queue with an specific order.
 void *insert is the address of the item you want to insert.
 int order is the order you want to give to this item.

 void RemoveDiskItem(void *insert);
 Used for removing an item from the disk queue.
 void *insert is the address of the item you want to remove.

 void RemoveDiskHead();
 Used for removing the item at the head of the disk queue.

 int DiskExists(void *insert);
 Used for checking whether a specific item exists in the disk queue or not.
 void *insert is the address of the item you want to check.
 If exist - return 1
 If not exist - return 0

 int DiskEmpty();
 Used for checking whether disk queue is empty or not.
 If empty - return 1
 If not empty - return 0

 void DiskQPrint();
 Used for printing the whole structure of disk queue.
 A function used for debugging.

 int DiskReadPID(int order);
 Used for reading the pid of PCB item at a specific location of disk queue.
 int order is the order of item you wnat to find.
 (For example, first item has order 0, second item has order 1, and so on)
 return the corresponding PID, if doesn't exist, return -1.

 char *DiskReadName(int order);
 Used for reading the name of PCB item at a specific location of disk queue.
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

int DiskID;//Used for storing QID
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


// Used for initializing the disk queue.
void InitialDisk()
{
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 3, DO_LOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult);//Lock
	DiskID = QCreate("DiskQ");//Create a queue called "DiskQ"
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 3, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult);//Unlock
}

// Used for inserting an item to the tail of the disk queue.
//void *insert is the address of the item you want to insert.
void InsertDiskTail(void *insert)
{
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 3, DO_LOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult);
	QInsertOnTail(DiskID, insert);//Insert the item to the tail of disk queue
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 3, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult);
}

//Used for inserting an item to the head of the disk queue.
//void *insert is the address of the item you want to insert.
void InsertDiskHead(void *insert)
{
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 3, DO_LOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult);
	QInsert(DiskID,0,insert);//Insert the item to the disk queue with an order of 0
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 3, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult);
}

//Used for inserting an item to the disk queue with an specific order.
//void *insert is the address of the item you want to insert.
//int order is the order you want to give to this item.
void InsertDiskOrder(void *insert, int order)
{
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 3, DO_LOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult);
	QInsert(DiskID, order, insert);//Insert the item to the disk queue with an order
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 3, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult);
}

// Used for removing an item from the disk queue.
//void *insert is the address of the item you want to remove.
void RemoveDiskItem(void *insert)
{
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 3, DO_LOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult);
	QRemoveItem(DiskID, insert);//Remove the item from the disk queue
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 3, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult);
}

// Used for removing the item at the head of the disk queue.
void RemoveDiskHead()
{
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 3, DO_LOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult);
	QRemoveHead(DiskID);//Remove the head item from the disk queue
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 3, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult);
}

//Used for checking whether a specific item exists in the disk queue or not.
//void *insert is the address of the item you want to check.
//If exist - return 1
//If not exist - return 0
int DiskExists(void *insert) 
{
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 3, DO_LOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult);
	int *exist = (int *)QItemExists(DiskID, insert);//Check whether this item exists in disk queue or not
	if (exist == -1)
	{
		//Not exist
		READ_MODIFY(MEMORY_INTERLOCK_BASE + 3, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,
			&LockResult);
		return 0;
	}
	else
	{
		//Exist
		READ_MODIFY(MEMORY_INTERLOCK_BASE + 3, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,
			&LockResult);
		return 1;
	}
}

// Used for checking whether disk queue is empty or not.
//If empty - return 1
//If not empty - return 0
int DiskEmpty() {
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 3, DO_LOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult);
	int *isempty=(int *)QNextItemInfo(DiskID);//Check whether the disk queue is empty or not
	if (isempty == -1)
	{
		//Empty
		READ_MODIFY(MEMORY_INTERLOCK_BASE + 3, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,
			&LockResult);
		return 1;
	}
	else
	{
		//Not empty
		READ_MODIFY(MEMORY_INTERLOCK_BASE + 3, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,
			&LockResult);
		return 0;
	}
}

//Used for printing the whole structure of disk queue.
//A function used for debugging.
void DiskQPrint() {
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 3, DO_LOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult);
	QPrint(DiskID);//Do the print
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 3, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult);
}

//Used for reading the pid of PCB item at a specific location of disk queue.
//int order is the order of item you wnat to find.
//(For example, first item has order 0, second item has order 1, and so on)
//return the corresponding PID, if doesn't exist, return -1.
int DiskReadPID(int order) {
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 3, DO_LOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult);
	struct PCB *op;//Used for storing the return value
	int *check = (int *)QWalk(DiskID, order);//Check whether the order is valid or not
	if (check==-1)
	{
		//Not exist
		READ_MODIFY(MEMORY_INTERLOCK_BASE + 3, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,
			&LockResult);
		return -1;
	}
	else
	{
		//Exist
		op = QWalk(DiskID, order);//Get the specific item from disk queue
	}
	int pid = op->pid;//Get the pid of returning PCB
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 3, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult);
	return pid;//Return the pid
}

// Used for reading the name of PCB item at a specific location of disk queue.
//int order is the order of item you wnat to find.
//(For example, first item has order 0, second item has order 1, and so on)
//return the corresponding name, if doesn't exist, return -1.
char *DiskReadName(int order) {
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 3, DO_LOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult);
	struct PCB *op;//Used for store the return value
	int *check = (int *)QWalk(DiskID, order);//Check whether the order is valid or not
	if (check == -1)
	{
		//Not exist
		READ_MODIFY(MEMORY_INTERLOCK_BASE + 3, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,
			&LockResult);
		return -1;
	}
	else
	{
		//Exist
		op = QWalk(DiskID, order);//Get the specific item from disk queue
	}
	char *name = op->name;//Get the name of PCB
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 3, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult);
	return name;//Return the name
}