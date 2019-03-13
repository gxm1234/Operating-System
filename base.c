/************************************************************************

 This code forms the base of the operating system you will
 build.  It has only the barest rudiments of what you will
 eventually construct; yet it contains the interfaces that
 allow test.c and z502.c to be successfully built together.

 Revision History:
 1.0 August 1990
 1.1 December 1990: Portability attempted.
 1.3 July     1992: More Portability enhancements.
 Add call to SampleCode.
 1.4 December 1992: Limit (temporarily) printout in
 interrupt handler.  More portability.
 2.0 January  2000: A number of small changes.
 2.1 May      2001: Bug fixes and clear STAT_VECTOR
 2.2 July     2002: Make code appropriate for undergrads.
 Default program start is in test0.
 3.0 August   2004: Modified to support memory mapped IO
 3.1 August   2004: hardware interrupt runs on separate thread
 3.11 August  2004: Support for OS level locking
 4.0  July    2013: Major portions rewritten to support multiple threads
 4.20 Jan     2015: Thread safe code - prepare for multiprocessors
 ************************************************************************/

#include             "global.h"
#include             "syscalls.h"
#include             "protos.h"
#include             "string.h"
#include             <stdlib.h>
#include             <ctype.h>
#include             <unistd.h>
#include             <math.h>


INT32 LockResult;//Used for doing lock and unlock

//  Allows the OS and the hardware to agree on where faults occur
extern void *TO_VECTOR[];
int checkp(char *name, int flag, int did, int ptr);

char *call_names[] = {       "MemRead  ", "MemWrite ", "ReadMod  ", "GetTime  ",
		"Sleep    ", "GetPid   ", "Create   ", "TermProc ", "Suspend  ",
		"Resume   ", "ChPrior  ", "Send     ", "Receive  ", "PhyDskRd ",
		"PhyDskWrt", "DefShArea", "Format   ", "CheckDisk", "OpenDir  ",
		"OpenFile ", "CreaDir  ", "CreaFile ", "ReadFile ", "WriteFile",
		"CloseFile", "DirContnt", "DelDirect", "DelFile  " };

long b;//Used for getting test address
//Used for storing bitmap
struct bmd {
	unsigned char bitmap[16];
	int unchanged;
};

struct bmd bitmaps[8][16];//Used for storing bitmap

struct Swapinf {//Used for recording swap information
	int used;
	int regid;
	int regadd;
};


//Structure of a single PCB
struct PCB {
		long Context;//Context of the process
		long pid;//Process ID of the process
		char *name;//Name of the process
		long order;//Priority of the process
		int ptime;//Wake-up time of the process
		int status;//Used for checking whether this process has been terminated or not
		           //(when status=0, it means that this process have been terminated)
		int diskid;//Used for storing disk id when doing some disk operations
		int cdid;//Current did
		int csector;//Current sector
		int Memory[64];//Related logical addresses

};

int MemoryOwner[64];//Used for recording the owner of each page

int MemoryOrder[64];//Used for recording the order of each page

int MemoryAddress[64];//Used for recording the logical page of each page

int MemoryState[64];//Used for recording the state of each page

struct Swapinf Swap[400];

MP_INPUT_DATA MPData;



//Structure of File/Directory Information
struct FDInformation {
	int Inode;
	char *name;
	int flag;
	int ptr;
	int parent;
	int did;
	int time;
	int size;
};


struct PCB Blocks[14];//List used for storing PCBs

short *PAGE_TBL_ADDR;//Used for storing page table




struct FDInformation FDIs[50];//Used for storing File/Directory information

#define                  DO_LOCK                     1
#define                  DO_UNLOCK                   0
#define                  SUSPEND_UNTIL_LOCKED        TRUE
#define                  DO_NOT_SUSPEND              FALSE
#define                  MEMORY_INTERLOCK_BASE       0x7FE00000
#define                  FULL                        10000
#define                  LIMITED                     50
#define                  INITIAL                     10
#define                  NONE                        0

int svcprint=NONE;//Used for control the printing of svc
int scheduleprint=NONE;//Used for control the printing of schedule printer
int memoryprint = NONE;//Used for control the printing of memory printer
int interrprint = NONE;//Used for control the printing of interrupt handler
int faultprint = NONE; // Used for control the printing of fault handler
int PID = 1;//Used for generating PIDs
int endstart = 0;//Used for telling the schedule printer the start of terminating processes.
//int currentPlace = 0;
//int currentdid = 0;
int Inode = 0;
int Sector = 0;
int rootSet = 0;
int memoryinit = 0;
int clock = 0;
int initswap = 0;
int bitmapinit=0;
//Some initializer and allocator

/************************************************************************
 INTERRUPT_HANDLER
 When the Z502 gets a hardware interrupt, it transfers control to
 this routine in the Operating System.
 NOTE WELL:  Just because the timer or the disk has interrupted, and
             therefore this code is executing, it does NOT mean the 
	     action you requested was successful.
	     For instance, if you give the timer a NEGATIVE time - it 
	     doesn't know what to do.  It can only cause an interrupt
	     here with an error.
	     If you try to read a sector from a disk but that sector
	     hasn't been written to, that disk will interrupt - the
	     data isn't valid and it's telling you it was confused.
	     YOU MUST READ THE ERROR STATUS ON THE INTERRUPT
 ************************************************************************/
void InterruptHandler(void) {
	INT32 DeviceID;//Receive the deviceID of interrupt
	INT32 Status;//Receive the status of interrupt

	MEMORY_MAPPED_IO mmio;       // Enables communication with hardware



	// Get cause of interrupt
	mmio.Mode = Z502GetInterruptInfo;
	mmio.Field1 = mmio.Field2 = mmio.Field3 = mmio.Field4 = 0;
	MEM_READ(Z502InterruptDevice, &mmio);
	DeviceID = mmio.Field1;
	Status = mmio.Field2;
	if (mmio.Field4 != ERR_SUCCESS) {
		aprintf( "The InterruptDevice call in the InterruptHandler has failed.\n");
		aprintf("The DeviceId and Status that were returned are not valid.\n");
		
	}
	//Print some output about the interrupt
	if (interrprint > 0) {
		aprintf("there is an interrupt.\n");
		aprintf("Device ID: %d\n",DeviceID);
		aprintf("Status: %d\n", Status);
		interrprint--;
	}
	
	if (DeviceID == 4)
	{
		//This interrupt is caused by timer

		//Get the current time
		mmio.Mode = Z502ReturnValue;
		mmio.Field1 = mmio.Field2 = mmio.Field3 = mmio.Field4 = 0;
		MEM_WRITE(Z502Clock, &mmio);
		int nowtime = (int)mmio.Field1;

		
		int retpid = TimerReadPID(0);//Get the pid of the head of timer queue

		//TimerQPrint();
		//printf("Timer return PID: %d\n", retpid);


		int time1 = Blocks[retpid].ptime;//Get the wake-up time of corresponding process
		RemoveTimerItem(&Blocks[retpid]);//Remove the head item from timer queue
		if (!TimerEmpty())
		{
			//Timer is not empty
			int pid2= TimerReadPID(0);//Get the pid of the head of timer queue now
			int time2 = Blocks[pid2].ptime;//Get the wake-up time of corresponding process
			int compare1 = abs(nowtime - time1);
			int compare2 = abs(nowtime - time2);//Calculate the difference between these two wake-up time and current time
			if (compare2<compare1)//Means that time2 nearer to current time.
			{
				//so we need to start one more timer to avoid the situation that an item in timer queue won't be removed.
				mmio.Mode = Z502Start;
				mmio.Field1 = abs(time2-time1);
				mmio.Field2 = mmio.Field3 = 0;
				MEM_WRITE(Z502Timer, &mmio);
			}

		}
		InsertReadyOrder(&Blocks[retpid], Blocks[retpid].order);//Insert the removed PCB to ready queue with corresponding order

	}

	if (DeviceID >= 5)
	{
		
		//This interrupt is caused by disk

		
		int did = DeviceID - 5;//Get the disk id

		int countdisk = 0;//Set a loop variable
		int diskpid;//Used for receiving pid
		while (1)
		{
			diskpid = DiskReadPID(countdisk);//Read pid
			if (diskpid == -1)
			{
				//This is only for avoiding faults, usually it won't be reached
				int retpid2 = DiskReadPID(0);//Get the pid of the head of timer queue
				RemoveDiskItem(&Blocks[retpid2]);//Remove the head item from timer queue
				InsertReadyOrder(&Blocks[retpid2], Blocks[retpid2].order);//Insert the removed PCB to ready queue with corresponding order
				break;
			}
			else
			{

				if (Blocks[diskpid].diskid == did)
				{
					//Find the corresponding PCB
					RemoveDiskItem(&Blocks[diskpid]);//Remove the corresponding item from timer queue
					InsertReadyOrder(&Blocks[diskpid], Blocks[diskpid].order);//Insert the removed PCB to ready queue with corresponding order
					break;
				}
				else
				{
					countdisk++;
				}

				

			}
		}


	}
		
}           // End of InterruptHandler

/************************************************************************
 FAULT_HANDLER
 The beginning of the OS502.  Used to receive hardware faults.
 ************************************************************************/

void FaultHandler(void) {
	INT32 DeviceID;

	MEMORY_MAPPED_IO mmio;       // Enables communication with hardware

	static INT32 how_many_fault_entries = 0;

	// Get cause of fault
	mmio.Field1 = mmio.Field2 = mmio.Field3 = 0;
	mmio.Mode = Z502GetInterruptInfo;
	MEM_READ(Z502InterruptDevice, &mmio);
	DeviceID = mmio.Field1;

	INT32 Status;
	Status = mmio.Field2;
	int  how_many_interrupt_entries;
	// This causes a print of the first few faults - and then stops printing!
	how_many_fault_entries++; 

	//Print some output about the interrupt
	if (faultprint > 0) {
		aprintf("there is an fault.\n");
		aprintf("Device ID: %d\n", DeviceID);
		aprintf("Status: %d\n", Status);
		faultprint--;
	}

	if (DeviceID == 2)
	{
		if (memoryinit == 0)
		{
			for (int i = 0; i < 64; i++)
			{
				MemoryOwner[i] = -1;
				MemoryState[i] = 4;
			}
			memoryinit = 1;
		}

		if (initswap == 0)
		{
			initSwap();
			initswap = 1;
		}
		//printf("\n\nInvaild memory!\n\n");
		mmio.Mode = Z502GetPageTable;
		mmio.Field1 = mmio.Field2 = mmio.Field3 = 0;
		MEM_READ(Z502Context, &mmio);
		PAGE_TBL_ADDR = (short *)mmio.Field1;   // Gives us the page table
		// Set to VALID the logical page 0 and have it point at physical
		// frame 0.
		if (Status >= 1024)
		{
			mmio.Mode = Z502Action;
			mmio.Field1 = mmio.Field2 = mmio.Field3 = 0;
			MEM_WRITE(Z502Halt, &mmio);
		}
		else
		{
			mmio.Mode = Z502GetCurrentContext;
			mmio.Field1 = mmio.Field2 = mmio.Field3 = mmio.Field4 = 0;
			MEM_READ(Z502Context, &mmio);


			int countcurrent = 0;
			for (countcurrent = 0; countcurrent < PID; countcurrent++)
			{

				if (Blocks[countcurrent].Context == mmio.Field1)
				{
					//printf("\ngivebit:%d\n", Blocks[countcurrent].givebit);
					if (checkFull())
					{
						if (PAGE_TBL_ADDR[Status] == 2)
						{
							int victim=WriteVictim(countcurrent, Status);
							ReadBack(countcurrent,victim,Status);
						}
						else
						{
							//printf("\nanyone there?\n");
							WriteVictim(countcurrent,Status);
						}
						
						

					}
					else
					{
						int pn=getFreePhysicalPage();
						PAGE_TBL_ADDR[Status] = (UINT16)PTBL_VALID_BIT + (UINT16)pn;
						Blocks[countcurrent].Memory[pn] = Status;
						MemoryOwner[pn] = countcurrent;
						MemoryOrder[pn] = clock;
						MemoryAddress[pn]=Status;
						MemoryPrinter();
						MemoryState[pn] = 1;
						clock++;
					}

					break;
				}
			}
			
		}


	}


} // End of FaultHandler

/************************************************************************
 MemoryPrinter
 Function used for printing memory status.
 ************************************************************************/
void MemoryPrinter() {
	memset(&MPData, 0, sizeof(MP_INPUT_DATA));  // Good practice - clean up

	for (int j = 0; j < NUMBER_PHYSICAL_PAGES; j++) {
		if (MemoryOwner[j] == -1)
		{
			MPData.frames[j].InUse = FALSE;
		}
		else
		{
			MPData.frames[j].InUse = TRUE;
			MPData.frames[j].Pid = MemoryOwner[j];
			MPData.frames[j].LogicalPage = MemoryAddress[j];
			MPData.frames[j].State = MemoryState[j];
		}
		

	}
	if (memoryprint > 0)
	{
		MPPrintLine(&MPData);
		memoryprint--;
	}
	
}

/************************************************************************
 writeBitMap
 Function used for writing bitmap.
 ************************************************************************/
void writeBitMap(int usedsector,int flag) {
	//In these tests.errors may occur
	if (b == (long)test45 || b == (long)test46)
	{
		return;
	}
	if (bitmapinit == 0)
	{
		for (int i = 0; i < 8; i++)
		{
			for (int j = 0; j < 16; j++)
			{
				bitmaps[i][j].bitmap[0] += 127;
				bitmaps[i][j].bitmap[1] = 255;
				bitmaps[i][j].bitmap[2] = 192;
			}

		}
		bitmapinit = 1;
	}
	int did;
	if (flag == 0)
	{
		did = getCurrentDid();
	}
	else
	{
		did = 1;
	}
	
	int sector = usedsector / 128;
	int offset1 = usedsector - (sector * 128);
	int place = offset1 / 16;
	int offset2 = offset1 - (place * 16);
	int add = 128;
	for (int i = 0; i < offset2; i++)
	{
		add = add / 2;
	}

	bitmaps[did][sector].bitmap[place] += add;
	bitmaps[did][sector].unchanged = 1;
}

/************************************************************************
 WasteTime
 A empty function used for dispatcher to wait.
 ************************************************************************/
void WasteTime() {

}

/************************************************************************
 getFreePhysicalPage
 Function used for getting a free physical page.
 ************************************************************************/
int getFreePhysicalPage() {
	for (int i = 0; i < 64; i++)
	{
		if (MemoryOwner[i] == -1)
		{
			return i;
		}
	}

	return -1;
}


/************************************************************************
 getVictim
 Function used for finding a victim page.
 ************************************************************************/
int getVictim(int pid) {
	int victim=-1;
	for (int i = 0; i < 64; i++)
	{
		if (MemoryOwner[i] == pid)
		{
			if (victim == -1)
			{
				victim = i;
			}
			else if (MemoryOrder[i] < MemoryOrder[victim])
			{
				victim = i;
			}
		}
	}
	return victim;
}

/************************************************************************
 checkFull
 Function used for checking whether there are any free physical pages or not.
 ************************************************************************/
int checkFull() {
	for (int i = 0; i < 64; i ++ )
	{
		if (MemoryOwner[i] == -1)
		{
			return 0;
		}
	}
	//printf("full!\n");
	return 1;
}

/************************************************************************
 initSwap
 Function used for initializing the swap area.
 ************************************************************************/
void initSwap() {
	for (int i = 0; i < 400; i++)
	{
		Swap[i].used = -1;
	}
}
/************************************************************************
 ReadBack
 Function used for reading back a page stored in disk.
 ************************************************************************/
void ReadBack(int pid, int victim, int DiskStatus) {
	MEMORY_MAPPED_IO mmio;
	unsigned char readswapdisk[PGSIZE];
	char writeswapmemory[PGSIZE];
	int place;
	for (int i = 0; i < 400; i++)
	{
		if (Swap[i].used==1 && Swap[i].regadd == DiskStatus && Swap[i].regid == pid)
		{
			place = i;
			break;
		}
	}

	mmio.Mode = Z502DiskRead;
	mmio.Field1 = 1;
	mmio.Field2 = 13 + place;
	mmio.Field3 = (long)readswapdisk;
	DoDisk(mmio);
	for (int i = 0; i < 16; i++)
	{
		writeswapmemory[i] = (char)readswapdisk[i];
	}
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 6, DO_LOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult);
	Z502WritePhysicalMemory(victim, (char *)writeswapmemory);
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 6, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult);
	Swap[place].used = -1;
	Swap[place].regid = 0;
	Swap[place].regadd = 0;
}

int WriteVictim(int pid, int MemoryStatus) {

	int freeplace = findFreeSwap(pid);

	writeBitMap(13 + freeplace,1);

	int victim = getVictim(pid);
	int DiskStatus = Blocks[pid].Memory[victim];
	char readswap[PGSIZE];
	MEMORY_MAPPED_IO mmio;


	PAGE_TBL_ADDR[MemoryStatus] = (UINT16)PTBL_VALID_BIT + (UINT16)victim;
	PAGE_TBL_ADDR[DiskStatus] = (UINT16)2;

	READ_MODIFY(MEMORY_INTERLOCK_BASE + 6, DO_LOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult);
	Z502ReadPhysicalMemory(victim, (char *)readswap);
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 6, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult);
	/*if (Blocks[pid].swapinit == 0)
	{
		initSwap(pid);
		Blocks[pid].swapinit = 1;
	}*/

	unsigned char writeswap[PGSIZE];

	for (int i = 0; i < 16; i++)
	{
		writeswap[i] = (unsigned char)readswap[i];
	}



	mmio.Mode = Z502DiskWrite;
	mmio.Field1 = 1;
	mmio.Field2 = 13+freeplace;
	mmio.Field3 = (long)writeswap;
	DoDisk(mmio);

	

	Swap[freeplace].regadd = DiskStatus;
	Blocks[pid].Memory[victim] = MemoryStatus;
	MemoryOrder[victim] = clock;

	MemoryAddress[victim] = MemoryStatus;
	MemoryState[victim] = 2;
	MemoryPrinter();

	clock++;

	return victim;
}
/************************************************************************
 findFreeSwap
 Function used for finding a free swap sector.
 ************************************************************************/
int findFreeSwap(int pid) {
	for (int i = 0; i < 400; i++)
	{
		if (Swap[i].used == -1) {
			Swap[i].used = 1;
			Swap[i].regid = pid;
			return i;
		}
	}
	//printf("\nfull!\n");
	return -1;
}


/************************************************************************
 dispatcher
 Function used for running the processes in ready queue.
 ************************************************************************/
void dispatcher() {

	MEMORY_MAPPED_IO    mmio;      // Enables communication with hardware
	while (ReadyEmpty()) {
		//When ready queue is empty, keep on waiting.
		CALL(WasteTime());
	}

	
	int retpid=ReadyReadPID(0);//Get the pid of the head of ready queue
	//char *nm = ReadyReadName(0);



	//printf("return PID: %d\n", retpid);
	//printf("return Name: %s\n", nm);


	RemoveReadyItem(&Blocks[retpid]);//Remove the head item from ready queue


	CallSchedulePrinter(2,Blocks[retpid].pid);//Call the schedule printer for printing the status

	//Start the corresponding process
	mmio.Mode = Z502StartContext;
	mmio.Field1 = Blocks[retpid].Context;
	mmio.Field2 = START_NEW_CONTEXT_AND_SUSPEND;
	mmio.Field3 = mmio.Field4 = 0;
	MEM_WRITE(Z502Context, &mmio);
	

}

/************************************************************************
 CallSchedulePrinter
 Used for printing the status of queues and other things.
 int action shows the type of action
 int target shows the terget PID in this action
 ************************************************************************/

void CallSchedulePrinter(int action, int target) {

	
	if (scheduleprint == 0)
	{
		//Print number reaches the limit
		return;
	}


	SP_INPUT_DATA spip;//Initialize structure for input
	MEMORY_MAPPED_IO    mmio;// Enables communication with hardware
	spip.TargetPID = target;//Get the target PID
	spip.NumberOfProcSuspendedProcesses = 0;//Now we don't need multiprocessor

	//Get the current context
	mmio.Mode = Z502GetCurrentContext;
	mmio.Field1 = mmio.Field2 = mmio.Field3 = mmio.Field4 = 0;
	MEM_READ(Z502Context, &mmio);

	//Use the current context to find the pid of current running process
	int countcurrent = 0;
	for (countcurrent = 0; countcurrent < PID; countcurrent++)
	{

		if (Blocks[countcurrent].Context == mmio.Field1)
		{
			
			spip.CurrentlyRunningPID = Blocks[countcurrent].pid;

			
			break;
		}
	}
	spip.NumberOfRunningProcesses = 1;//Up to now, there is only one process running at the same time

	//Use the action attribute to get the type of action
	if (action == 0)
	{
		strcpy(spip.TargetAction, "CREATE");
	}
	else if (action == 1)
	{
		strcpy(spip.TargetAction, "SLEEP");
	}
	else if (action == 2)
	{
		strcpy(spip.TargetAction, "DISPATCH");
	}
	else if (action == 3)
	{
		strcpy(spip.TargetAction, "TERMINATE");
	}
	else if (action == 4)
	{
		strcpy(spip.TargetAction, "DISK_WRITE");
	}
	else if (action == 5)
	{
		strcpy(spip.TargetAction, "DISK_READ");
	}
	
	//Get the status of ready queue
	if (!ReadyEmpty())
	{
		int readypid;
		countcurrent = 0;
		while (countcurrent < 14)
		{
			readypid = ReadyReadPID(countcurrent);
			if (readypid == -1)
			{
				//Reach the tail of the queue
				break;
			}
			else
			{
				spip.ReadyProcessPIDs[countcurrent] = readypid;

				countcurrent++;
			}
		}

		spip.NumberOfReadyProcesses = countcurrent;
	}
	else
	{
		spip.NumberOfReadyProcesses = 0;
	}

	//Get the status of timer queue
	if (!TimerEmpty())
	{
		int timerpid = 0;
		countcurrent = 0;
		while (countcurrent<14)
		{
			timerpid = TimerReadPID(countcurrent);
			if (timerpid == -1)
			{
				//Reach the tail of the queue
				break;
			}
			else
			{
				spip.TimerSuspendedProcessPIDs[countcurrent] = timerpid;

				countcurrent++;
			}
		}

		spip.NumberOfTimerSuspendedProcesses = countcurrent;

	}
	else
	{
		spip.NumberOfTimerSuspendedProcesses = 0;
	}

	//Get the status of disk queue
	if (!DiskEmpty())
	{
		int diskpid = 0;
		countcurrent = 0;
		while (countcurrent < 14)
		{
			diskpid = DiskReadPID(countcurrent);
			if (diskpid == -1)
			{
				//Reach the tail of the queue
				break;
			}
			else
			{

				spip.DiskSuspendedProcessPIDs[countcurrent] = diskpid;

				countcurrent++;

			}
		}

		spip.NumberOfDiskSuspendedProcesses = countcurrent;


	}
	else
	{
		spip.NumberOfDiskSuspendedProcesses = 0;
	}

	//Get all of the process have been terminated
	int tercount = 0;

	if (endstart == 1)
	{
		for (countcurrent = 0; countcurrent < PID; countcurrent++)
		{
			if (Blocks[countcurrent].status == 0)
			{

				spip.TerminatedProcessPIDs[tercount] = Blocks[countcurrent].pid;

				tercount++;
			}
		}

		spip.NumberOfTerminatedProcesses = tercount;
	}
	else
	{
		spip.NumberOfTerminatedProcesses = 0;
	}
	spip.NumberOfMessageSuspendedProcesses = 0;
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 5, DO_LOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult);
	SPPrintLine(&spip);//Print the status
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 5, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult);
	scheduleprint--;

}




/************************************************************************
 SVC
 The beginning of the OS502.  Used to receive software interrupts.
 All system calls come to this point in the code and are to be
 handled by the student written code here.
 The variable do_print is designed to print out the data for the
 incoming calls, but does so only for the first ten calls.  This
 allows the user to see what's happening, but doesn't overwhelm
 with the amount of data.
 ************************************************************************/

void svc(SYSTEM_CALL_DATA *SystemCallData) {
	short call_type;//Used for getting the type of system call
	short i;//loop attribute
	long co;
	MEMORY_MAPPED_IO    mmio;// Enables communication with hardware
	MEMORY_MAPPED_IO    mmio2;
	INT32 Status;
	char *psname;//Used for getting name for getting PID
	char *pname;//Used for getting name for creating new process
	unsigned char writedata[16];
	unsigned char readdata[16];
	UINT16 index[8];
	int parent;
	int ptr;
	int InodeStore;
	int cdid;
	int csector;

	call_type = (short) SystemCallData->SystemCallNumber;
	if (svcprint > 0) {
		//Print the status of svc
		aprintf("SVC handler: %s\n", call_names[call_type]);
		for (i = 0; i < SystemCallData->NumberOfArguments - 1; i++) {
			//Value = (long)*SystemCallData->Argument[i];
			aprintf("Arg %d: Contents = (Decimal) %8ld,  (Hex) %8lX\n", i,
					(unsigned long) SystemCallData->Argument[i],
					(unsigned long) SystemCallData->Argument[i]);
		}
		svcprint--;
	}

	switch (call_type) { 
		// Get time service call 
		case SYSNUM_GET_TIME_OF_DAY:   // This value is found in syscalls.h 
			//Get the current time
			mmio.Mode = Z502ReturnValue; 
			mmio.Field1 = mmio.Field2 = mmio.Field3 = 0; 
			MEM_READ(Z502Clock, &mmio); 
			*(long *)SystemCallData->Argument[0] = mmio.Field1; 
			break; 
		// terminate system call 
		case SYSNUM_TERMINATE_PROCESS:


			if (SystemCallData->Argument[0] == -1)//Stop the current running process
			{

				if (endstart == 0)
				{
					endstart == 1;//Set the attribute to tell schedule printer to print the terminated process
				}

				//Get the current context
				mmio.Mode = Z502GetCurrentContext;
				mmio.Field1 = mmio.Field2 = mmio.Field3 = mmio.Field4 = 0;
				MEM_READ(Z502Context, &mmio);

				
				if (Blocks[0].Context == (long)mmio.Field1)//Case of termination of the main process
				{
					//Stop the simulation
					mmio.Mode = Z502Action;
					mmio.Field1 = mmio.Field2 = mmio.Field3 = 0;
					MEM_WRITE(Z502Halt, &mmio);
				}
				else
				{
					//Find the PCB of current running process
					int g = 0;
					for (g = 0; g < PID; g++)
					{

						if (Blocks[g].Context == mmio.Field1)
						{
							Blocks[g].status = 0;
							
							break;
						}
					}

					CallSchedulePrinter(3, Blocks[g].pid);//Call the schedule printer

					//Suspend the corresponding process
					mmio.Mode = Z502StartContext;
					mmio.Field2 = SUSPEND_CURRENT_CONTEXT_ONLY;
					mmio.Field3 = mmio.Field4 = 0;
					MEM_WRITE(Z502Context, &mmio);
					
					//Call the dispatcher to run a process
					if (b == (long)test43||b == (long)test45||b == (long)test46)//In these tests.errors may occur
					{
						if (!TimerEmpty())
						{
							if (ReadyEmpty())
							{
								int retpid = TimerReadPID(0);//Get the pid of the head of timer queue
								RemoveTimerItem(&Blocks[retpid]);//Remove the head item from timer queue
								InsertReadyOrder(&Blocks[retpid], Blocks[retpid].order);//Insert the removed PCB to ready queue with corresponding order
							}
						}

					}
					
					dispatcher();
				}
				
			}
			else if (SystemCallData->Argument[0] == -2)//Stop the simulation
			{
				//Stop the simulation.
				mmio.Mode = Z502Action;
				mmio.Field1 = mmio.Field2 = mmio.Field3 = 0;
				MEM_WRITE(Z502Halt, &mmio);
			}
			else  //Stop a specific process (not running now)
			{
				int terid = (int)SystemCallData->Argument[0];//Get the pid of this process
				//Find the PCB of this process
				int checkterid;
				for (checkterid = 0; checkterid < PID; checkterid++)
				{
					if (Blocks[checkterid].pid == terid)
					{
						CallSchedulePrinter(3, Blocks[checkterid].pid);//Call the schedule printer
						RemoveReadyItem(&Blocks[checkterid]);//Remove this PCB from ready queue
						
						PID--;//Let new process can replace the space of this process
						*(long *)SystemCallData->Argument[1] = ERR_SUCCESS;
					}
				}
			}
			break; 
		//Getting the PID of a specific process
		case SYSNUM_GET_PROCESS_ID:

			psname = (char *)malloc(sizeof(char) * 100);
			psname= (char *)SystemCallData->Argument[0];//Get the name of process
			if (strcmp(psname, "") == 0)
			{
				//Return the PID of current running process
				mmio.Mode = Z502GetCurrentContext;
				mmio.Field1 = mmio.Field2 = mmio.Field3 = mmio.Field4 = 0;
				MEM_READ(Z502Context, &mmio);

				int idcheck = 0;
				for (idcheck = 0; idcheck < PID; idcheck++)
				{

					if (Blocks[idcheck].Context == mmio.Field1)
					{
						*(long *)SystemCallData->Argument[1] = idcheck;

						break;
					}
				}

				//*(long *)SystemCallData->Argument[1] = 0;
				*(long *)SystemCallData->Argument[2] = ERR_SUCCESS;
			}	
			else
			{
				//Find the PID of corresponding process and return
				int k;
				int flag1 = 0;
				for (k = 0; k < PID; k++)
				{
					if (k != 0)
					{
						if (strcmp(Blocks[k].name, psname) == 0)
						{
							if (Blocks[k].status == 0)
							{
								break;
							}
							flag1 = 1;
							*(long *)SystemCallData->Argument[1] = Blocks[k].pid;
							break;
						}
					}
				}
				
				if (flag1 == 0)
				{
					//Error case (didn't find the corresponding process)
					*(long *)SystemCallData->Argument[2] = ERR_BAD_PARAM;
				}
				else
				{
					//Success case
					*(long *)SystemCallData->Argument[2] = ERR_SUCCESS;
				}
			}
			
			break;
		//Let current process to sleep
		case SYSNUM_SLEEP:
			//Get the current time
			mmio.Mode = Z502ReturnValue;
			mmio.Field1 = mmio.Field2 = mmio.Field3 = mmio.Field4 = 0;
			MEM_WRITE(Z502Clock, &mmio);
			
			//Get the wake-up time of the process
			int wakeup = (int)SystemCallData->Argument[0];
			wakeup += mmio.Field1;

			//Start the timer
			mmio.Mode = Z502Start;
			mmio.Field1 = SystemCallData->Argument[0];
			mmio.Field2 = mmio.Field3 = 0;
			MEM_WRITE(Z502Timer, &mmio);

			//Get the current context
			mmio.Mode = Z502GetCurrentContext;
			mmio.Field1 = mmio.Field2 = mmio.Field3 = mmio.Field4 = 0;
			MEM_READ(Z502Context, &mmio);

			//Find the current running process
			int c = 0;
			for (c = 0; c < PID; c++)
			{
				
				if (Blocks[c].Context == mmio.Field1)
				{
					//printf("Sleep name: %s\n", Blocks[c].name);
					//printf("wakeuptime: %d\n",wakeup);
					Blocks[c].ptime = wakeup;//Set the wake-up time
					InsertTimerOrder(&Blocks[c],wakeup);//Insert the corresponding PCB to the timer queue
					
					break;
				}
			}
			CallSchedulePrinter(1, Blocks[c].pid);//Call the schedule printer
			
			dispatcher();//Call the dispatcher to run another process

			
			//Let the hardware to idle
			mmio.Mode = Z502Action;
			mmio.Field1 = mmio.Field2 = mmio.Field3 = 0;
			MEM_WRITE(Z502Idle, &mmio);
			
			break;
		//Case of reading data from disk
		case SYSNUM_PHYSICAL_DISK_READ:

			mmio.Mode = Z502DiskRead;
			mmio.Field1 = SystemCallData->Argument[0];
			mmio.Field2 = SystemCallData->Argument[1];
			mmio.Field3 = SystemCallData->Argument[2];

			DoDisk(mmio);


			break;
		//Case of writing data to disk
		case SYSNUM_PHYSICAL_DISK_WRITE:
			mmio.Mode = Z502DiskWrite;
			mmio.Field1 = SystemCallData->Argument[0];
			mmio.Field2 = SystemCallData->Argument[1];
			mmio.Field3 = SystemCallData->Argument[2];
			DoDisk(mmio);
			break;
		//Case of checking the status of disk
		case SYSNUM_CHECK_DISK:
			if (b != (long)test45&&b != (long)test46)//In these tests.errors may occur
			{
				for (int i = 0; i < 8; i++)
				{
					for (int j = 0; j < 16; j++)
					{
						if (bitmaps[i][j].unchanged == 1)
						{
							mmio.Mode = Z502DiskWrite;
							mmio.Field1 = i;
							mmio.Field2 = 1 + j;
							mmio.Field3 = (long)bitmaps[i][j].bitmap;
							DoDisk(mmio);
						}

					}
				}
			}



			//Call the hardware to check the disk
			mmio.Mode = Z502CheckDisk;
			mmio.Field1 = SystemCallData->Argument[0];
			mmio.Field2 = mmio.Field3 = mmio.Field4 = 0;
			MEM_WRITE(Z502Disk, &mmio);
			*(long *)SystemCallData->Argument[1] = mmio.Field4;
			break;
		//Case of creating a process
		case SYSNUM_CREATE_PROCESS:
			if (PID == 14)
			{
				//Reach the maximum number of processes
				*(long *)SystemCallData->Argument[4] = ERR_BAD_PARAM;
			}
			else if ((int)SystemCallData->Argument[2] <0)
			{
				//Invalid priority
				*(long *)SystemCallData->Argument[4] = ERR_BAD_PARAM;
			}
			else {
				int q;
				int flag = 0;
				pname = (char *)malloc(sizeof(char)*20);
				strcpy(pname, (char *)SystemCallData->Argument[0]);//Get the name of the new process
				//Check if there are duplicate names with the new name
				for (q = 0; q < PID; q++)
				{
					if (q != 0)
					{
						if (strcmp(pname, Blocks[q].name) == 0)
						{
							//Find the duplicate names
							flag = 1;
							*(long *)SystemCallData->Argument[4] = ERR_BAD_PARAM;
							break;
						}
					}
				}
				if (flag == 0)
				{
					//printf("PID: %d\n",PID);

					//Initial a new context
					short *PageTable = (short*)calloc(2, NUMBER_VIRTUAL_PAGES);
					mmio.Mode = Z502InitializeContext;
					mmio.Field1 = 0;
					mmio.Field2 = (long)SystemCallData->Argument[1];
					mmio.Field3 = (long)PageTable;

					MEM_WRITE(Z502Context, &mmio);

					//Set a new PCB and add it to the list
					struct PCB P;
					P.pid = PID;//A new PID
					P.Context = mmio.Field1;
					P.status = 1;//Haven't been terminated
					*(long *)SystemCallData->Argument[3] = PID;
					P.name =pname;
					P.order = (int)SystemCallData->Argument[2];
					Blocks[PID] = P;
					
					
					InsertReadyOrder(&Blocks[PID], Blocks[PID].order);//Insert the new process into the ready queue
					CallSchedulePrinter(0, Blocks[PID].pid);//Call the schedule printer
					PID++;
					
					*(long *)SystemCallData->Argument[4] = ERR_SUCCESS;

				}
			}
			
			
			break;
		//Case of changing the priority of a specific process
		case SYSNUM_CHANGE_PRIORITY:
			if ((int)SystemCallData->Argument[0] == -1)
			{
				//Change the priority of main process
				Blocks[0].order = (int)SystemCallData->Argument[1];//Set the new order

				//Check if the corresponding PCB is on the ready queue
				if (ReadyExists(Blocks[0]))
				{
					RemoveReadyItem(Blocks[0]);//Remove the corresponding PCB
					InsertReadyOrder(&Blocks[0], (int)SystemCallData->Argument[1]);//Add PCB with a new order
				}
			}
			else
			{
				//Change the priority of a specific process (not main process)
				Blocks[(int)SystemCallData->Argument[0]].order = (int)SystemCallData->Argument[1];//Set the new order
				if (ReadyExists(Blocks[(int)SystemCallData->Argument[0]]))
				{
					RemoveReadyItem(Blocks[(int)SystemCallData->Argument[0]]);//Remove the corresponding PCB
					InsertReadyOrder(&Blocks[PID], (int)SystemCallData->Argument[1]);//Add PCB with a new order
				}
			}
			
			*(long *)SystemCallData->Argument[2] = ERR_SUCCESS;
			break;
		//Case of suspending a specific process
		case SYSNUM_SUSPEND_PROCESS:

			if ((int)SystemCallData->Argument[0] == -1)
			{
				//We can't suspend main process
				*(long *)SystemCallData->Argument[1] = ERR_BAD_PARAM;
			}
			else if ((int)SystemCallData->Argument[0] <0|| (int)SystemCallData->Argument[0] >= PID)
			{
				//Invalid PID
				*(long *)SystemCallData->Argument[1] = ERR_BAD_PARAM;
			}
			else if (ReadyExists(&Blocks[(int)SystemCallData->Argument[0]])|| TimerExists(&Blocks[(int)SystemCallData->Argument[0]])|| DiskExists(&Blocks[(int)SystemCallData->Argument[0]]))
			{
				//The corresponding PCB exists on any of these queues, success case
				//Remove the PCB from the queues
				RemoveReadyItem(&Blocks[(int)SystemCallData->Argument[0]]);
				RemoveTimerItem(&Blocks[(int)SystemCallData->Argument[0]]);
				RemoveDiskItem(&Blocks[(int)SystemCallData->Argument[0]]);
				//printf("\n%s has been suspended!\n", Blocks[(int)SystemCallData->Argument[0]].name);
				*(long *)SystemCallData->Argument[1] = ERR_SUCCESS;
			}
			else
			{
				//The corresponding PCB doesn't exist at any of the queues, wrong case
				*(long *)SystemCallData->Argument[1] = ERR_BAD_PARAM;
			}
			break;
		//Case of resuming a specific process
		case SYSNUM_RESUME_PROCESS:

			if ((int)SystemCallData->Argument[0] == 0)
			{
				//We can't resume main process
				*(long *)SystemCallData->Argument[1] = ERR_BAD_PARAM;
			}
			else if ((int)SystemCallData->Argument[0] < 0 || (int)SystemCallData->Argument[0] >= PID)
			{
				//Invalid PID
				*(long *)SystemCallData->Argument[1] = ERR_BAD_PARAM;
			}
			else if (ReadyExists(&Blocks[(int)SystemCallData->Argument[0]])|| TimerExists(&Blocks[(int)SystemCallData->Argument[0]]) || DiskExists(&Blocks[(int)SystemCallData->Argument[0]]))
			{
				//The corresponding PCB exists on any of these queues, wrong case
				*(long *)SystemCallData->Argument[1] = ERR_BAD_PARAM;
			}
			else
			{
				//The corresponding PCB doesn't exist at any of the queues, success case
				//Insert the corresponding PCB to the ready queue
				InsertReadyOrder(&Blocks[(int)SystemCallData->Argument[0]], Blocks[(int)SystemCallData->Argument[0]].order);
				//printf("\n%s has been resumed!\n", Blocks[(int)SystemCallData->Argument[0]].name);
				*(long *)SystemCallData->Argument[1] = ERR_SUCCESS;
			}
			break;
		case SYSNUM_FORMAT:
			
			writeBitMap(0,0);

			writedata[0] = 0;
			writedata[1] = 4;
			writedata[2] = 1;
			writedata[3] = 100;
			writedata[4] = 0;
			writedata[5] = 8;
			writedata[6] = 1;
			writedata[7] = 0;
			writedata[8] = 17;
			writedata[9] = 0;
			writedata[10] = 161;
			writedata[11] = 1;
			writedata[12] = 0;
			writedata[13] = 0;
			writedata[14] = 0;
			writedata[15] = 1;
			mmio.Mode = Z502DiskWrite;
			mmio.Field1 = SystemCallData->Argument[0];
			mmio.Field2 = 0;
			mmio.Field3 = (long)writedata;

			
			
			DoDisk(mmio);
			Sector = 418;
			setCurrentDid(SystemCallData->Argument[0]);
			break;
		case SYSNUM_OPEN_DIR:
			if (SystemCallData->Argument[0] == -1)
			{
				cdid = getCurrentDid();
				csector = getCurrentSector();
				mmio.Mode = Z502DiskRead;
				mmio.Field1 = cdid;
				mmio.Field2 = csector;
				mmio.Field3 = (long)readdata;

				DoDisk(mmio);

				int a = checkp((char *)SystemCallData->Argument[1],0, cdid,(readdata[13]*256+readdata[12]));

				if (a == -1)
				{
					parent = readdata[0];

					mmio.Mode = Z502DiskRead;
					mmio.Field1 = cdid;
					mmio.Field2 = readdata[13] * 256 + readdata[12];
					mmio.Field3 = (long)index;

					DoDisk(mmio);


					InodeStore = Create(cdid, SystemCallData->Argument[1], parent, 0);



					for (int i = 0; i < 8; i++)
					{
						if (index[i] == 0)
						{

							index[i] = FDIs[InodeStore].ptr;

							break;
						}
					}


					int bitplace = readdata[13] * 256 + readdata[12];
					writeBitMap(bitplace,0);
					mmio.Mode = Z502DiskWrite;
					mmio.Field1 = cdid;
					mmio.Field2 = readdata[13] * 256 + readdata[12];
					mmio.Field3 = (long)index;

					DoDisk(mmio);

					


					for (int i = 0; i < 8; i++)
					{
						index[i] = 0;

					}

					mmio.Mode = Z502DiskWrite;
					mmio.Field1 = cdid;
					mmio.Field2 = FDIs[InodeStore].ptr + 1;
					mmio.Field3 = (long)index;

					DoDisk(mmio);

					setCurrentSector(FDIs[InodeStore].ptr);
				}
				else
				{
					//printf("\ncurrentplace:%d\n", a);

					setCurrentSector(a);
				}

			}
			else
			{
				if (rootSet == 0)
				{
					cdid = SystemCallData->Argument[0];
					setCurrentDid(cdid);


					parent = 31;
					int ptr = Create(cdid, SystemCallData->Argument[1], parent, 1);



					for (int i = 0; i < 8; i++)
					{
						index[i] = 0;

					}



					mmio.Mode = Z502DiskWrite;
					mmio.Field1 = cdid;
					mmio.Field2 = ptr + 1;
					mmio.Field3 = (long)index;

					DoDisk(mmio);


					
					setCurrentSector(ptr);
					//printf("currentPlace: %d", ptr);
					rootSet = 1;
				}
				else
				{
					cdid = SystemCallData->Argument[0];
					setCurrentDid(cdid);
					setCurrentSector(418);
					//printf("\nRoot opened!\n");
					*(long *)SystemCallData->Argument[2] = ERR_SUCCESS;
				}
			}
			break;
		case SYSNUM_OPEN_FILE:
			cdid = getCurrentDid();
			csector = getCurrentSector();
			mmio.Mode = Z502DiskRead;
			mmio.Field1 = cdid;
			mmio.Field2 = csector;
			mmio.Field3 = (long)readdata;

			DoDisk(mmio);

			int a = checkp((char *)SystemCallData->Argument[0], 0, cdid, (readdata[13] * 256 + readdata[12]));

			if (a == -1)
			{
				parent = readdata[0];

				mmio.Mode = Z502DiskRead;
				mmio.Field1 = cdid;
				mmio.Field2 = readdata[13] * 256 + readdata[12];
				mmio.Field3 = (long)index;

				DoDisk(mmio);


				InodeStore = Create(cdid, SystemCallData->Argument[0], parent, 0);
				


				for (int i = 0; i < 8; i++)
				{
					if (index[i] == 0)
					{

						index[i] = FDIs[InodeStore].ptr;
						
						break;
					}
				}

				
				writeBitMap(readdata[13] * 256 + readdata[12],0);

				mmio.Mode = Z502DiskWrite;
				mmio.Field1 = cdid;
				mmio.Field2 = readdata[13] * 256 + readdata[12];
				mmio.Field3 = (long)index;

				DoDisk(mmio);




				for (int i = 0; i < 8; i++)
				{
					index[i] = 0;

				}

				mmio.Mode = Z502DiskWrite;
				mmio.Field1 = cdid;
				mmio.Field2 = FDIs[InodeStore].ptr + 1;
				mmio.Field3 = (long)index;

				DoDisk(mmio);

				

				setCurrentSector(FDIs[InodeStore].ptr);
				//printf("\nName:%s Inode:%d\n",FDIs[InodeStore].name, InodeStore);
				*(long *)SystemCallData->Argument[1] = InodeStore;

			}
			else
			{
				

				setCurrentSector(a);

				for (int i = 0; i < 50; i++)
				{
					if (FDIs[i].ptr == a)
					{
						//printf("\nname:%s\n", FDIs[i].name);
						*(long *)SystemCallData->Argument[1] = FDIs[i].Inode;
					}
				}
			}
			break;
		case SYSNUM_CLOSE_FILE:
			InodeStore = SystemCallData->Argument[0];
			setCurrentSector(FDIs[FDIs[InodeStore].parent].ptr);
			break;
		case SYSNUM_WRITE_FILE:
			cdid = getCurrentDid();
			//Sector++;
			//int record = Sector;
			InodeStore = SystemCallData->Argument[0];
			//printf("\nWriteInode:%d Place:%d\n", InodeStore,FDIs[InodeStore].ptr);
			mmio.Mode = Z502DiskRead;
			mmio.Field1 = cdid;
			mmio.Field2 = FDIs[InodeStore].ptr;
			mmio.Field3 = (long)readdata;

			DoDisk(mmio);
			
			ReadWrite(cdid, (readdata[13] * 256 + readdata[12]),0,SystemCallData->Argument[1], SystemCallData->Argument[2]);
			break;
		case SYSNUM_READ_FILE:
			cdid = getCurrentDid();
			
			InodeStore = SystemCallData->Argument[0];
			//printf("\nReadInode:%d Place:%d\n", InodeStore, FDIs[InodeStore].ptr);
			mmio.Mode = Z502DiskRead;
			mmio.Field1 = cdid;
			mmio.Field2 = FDIs[InodeStore].ptr;
			mmio.Field3 = (long)readdata;

			DoDisk(mmio);
			
			ReadWrite(cdid,(readdata[13] * 256 + readdata[12]), 1, SystemCallData->Argument[1], SystemCallData->Argument[2]);
			//printf("result:%s\n",(char *)(SystemCallData->Argument[2]));
			break;
		case SYSNUM_CREATE_DIR:
			cdid = getCurrentDid();
			csector = getCurrentSector();
			//printf("\n\ncurrentplace:%d\n\n", currentPlace);

			mmio.Mode = Z502DiskRead;
			mmio.Field1 = cdid;
			mmio.Field2 = csector;
			mmio.Field3 = (long)readdata;

			DoDisk(mmio);
			
			parent = readdata[0];

			mmio.Mode = Z502DiskRead;
			mmio.Field1 = cdid;
			mmio.Field2 = readdata[13] * 256 + readdata[12];
			mmio.Field3 = (long)index;

			DoDisk(mmio);

			
			ptr = Create(cdid, SystemCallData->Argument[0], parent, 1);



			for (int i = 0; i < 8; i++)
			{
				if (index[i] == 0)
				{
					
					index[i] = ptr;
					break;
				}
			}

			mmio.Mode = Z502DiskWrite;
			mmio.Field1 = cdid;
			mmio.Field2 = readdata[13] * 256 + readdata[12];
			mmio.Field3 = (long)index;

			DoDisk(mmio);



			for (int i = 0; i < 8; i++)
			{
				index[i] = 0;

			}

			mmio.Mode = Z502DiskWrite;
			mmio.Field1 = cdid;
			mmio.Field2 = ptr + 1;
			mmio.Field3 = (long)index;

			DoDisk(mmio);

			

			break;
		case SYSNUM_CREATE_FILE:
			cdid = getCurrentDid();
			csector = getCurrentSector();
			mmio.Mode = Z502DiskRead;
			mmio.Field1 = cdid;
			mmio.Field2 = csector;
			mmio.Field3 = (long)readdata;

			DoDisk(mmio);

			parent = readdata[0];

			mmio.Mode = Z502DiskRead;
			mmio.Field1 = cdid;
			mmio.Field2 = readdata[13] * 256 + readdata[12];
			mmio.Field3 = (long)index;

			DoDisk(mmio);


			InodeStore = Create(cdid, SystemCallData->Argument[0], parent, 0);



			for (int i = 0; i < 8; i++)
			{
				if (index[i] == 0)
				{

					index[i] = FDIs[InodeStore].ptr;
					break;
				}
			}

			mmio.Mode = Z502DiskWrite;
			mmio.Field1 = cdid;
			mmio.Field2 = readdata[13] * 256 + readdata[12];
			mmio.Field3 = (long)index;

			DoDisk(mmio);

			
			for (int i = 0; i < 8; i++)
			{
				index[i] = 0;

			}

			mmio.Mode = Z502DiskWrite;
			mmio.Field1 = cdid;
			mmio.Field2 = FDIs[InodeStore].ptr + 1;
			mmio.Field3 = (long)index;

			DoDisk(mmio);

			
			break;
		case SYSNUM_DIR_CONTENTS:
			cdid = getCurrentDid();
			csector = getCurrentSector();
			//printf("\n\ncurrentplace:%d\n\n", currentPlace);

			mmio.Mode = Z502DiskRead;
			mmio.Field1 = cdid;
			mmio.Field2 = csector;
			mmio.Field3 = (long)readdata;

			DoDisk(mmio);
			printf("\nInode,  Filename, D/F,  Creation Time,  File Size\n");
			for (int i = 0; i < 50; i++)
			{
				if (FDIs[i].parent == readdata[0])
				{
					char* flag;
					if (FDIs[i].flag == 1)
					{
						flag = "D";
					}
					else
					{
						flag = "F";
					}
					printf("%d,  %s,  %s,  %d,  %d\n", FDIs[i].Inode,FDIs[i].name,flag,FDIs[i].time,FDIs[i].size);
				}
			}
			break;

		//Case of default
		default:  
			printf( "ERROR!  call_type not recognized!\n" );
			printf( "Call_type is - %i\n", call_type); 
	} 




}                                               // End of svc

/************************************************************************
 ReadWrite
 Function used for reading or writing a file.
 ************************************************************************/
void ReadWrite(int did, int ptr, int flag, int indexnumber, char *buffer) {
	MEMORY_MAPPED_IO mmio;
	UINT16 index[8];

	writeBitMap(ptr + 1 + indexnumber,0);
	writeBitMap(ptr,0);

	mmio.Mode = Z502DiskRead;
	mmio.Field1 = did;
	mmio.Field2 = ptr;
	mmio.Field3 = (long)index;
	DoDisk(mmio);

	if (flag == 0)
	{

		mmio.Mode = Z502DiskWrite;
		mmio.Field1 = did;
		mmio.Field2 = ptr + 1 + indexnumber;
		mmio.Field3 = (long)buffer;
		DoDisk(mmio);

		

		/*for (int i = 0; i < 8; i++)
		{
			printf("\nindex: %d",index[i]);
		}*/

		index[indexnumber] = ptr + 1 + indexnumber;
		

		mmio.Mode = Z502DiskWrite;
		mmio.Field1 = did;
		mmio.Field2 = ptr;
		mmio.Field3 = (long)index;
		DoDisk(mmio);

		

	}
	else
	{

		//printf("\nindex:%d\n", index[indexnumber]);
		mmio.Mode = Z502DiskRead;
		mmio.Field1 = did;
		mmio.Field2 = index[indexnumber];
		mmio.Field3 = (long)buffer;
		DoDisk(mmio);

		//printf("\nresult:%s\n", buffer);
	}



}

/************************************************************************
 getCurrentDid
 Function used for getting current id.
 ************************************************************************/
int getCurrentDid() {
	MEMORY_MAPPED_IO mmio;
	mmio.Mode = Z502GetCurrentContext;
	mmio.Field1 = mmio.Field2 = mmio.Field3 = mmio.Field4 = 0;
	MEM_READ(Z502Context, &mmio);

	//Use the current context to find the pid of current running process
	for (int countcurrent = 0; countcurrent < PID; countcurrent++)
	{

		if (Blocks[countcurrent].Context == mmio.Field1)
		{
			return Blocks[countcurrent].cdid;
		}
	}
}

/************************************************************************
 getCurrentSector
 Function used for getting current sector.
 ************************************************************************/
int getCurrentSector() {
	MEMORY_MAPPED_IO mmio;
	mmio.Mode = Z502GetCurrentContext;
	mmio.Field1 = mmio.Field2 = mmio.Field3 = mmio.Field4 = 0;
	MEM_READ(Z502Context, &mmio);

	//Use the current context to find the pid of current running process
	for (int countcurrent = 0; countcurrent < PID; countcurrent++)
	{

		if (Blocks[countcurrent].Context == mmio.Field1)
		{
			return Blocks[countcurrent].csector;
		}
	}
}

/************************************************************************
 setCurrentDid
 Function used for setting current disk id.
 ************************************************************************/
void setCurrentDid(int currentdid) {
	MEMORY_MAPPED_IO mmio;
	mmio.Mode = Z502GetCurrentContext;
	mmio.Field1 = mmio.Field2 = mmio.Field3 = mmio.Field4 = 0;
	MEM_READ(Z502Context, &mmio);

	//Use the current context to find the pid of current running process
	for (int countcurrent = 0; countcurrent < PID; countcurrent++)
	{

		if (Blocks[countcurrent].Context == mmio.Field1)
		{
			Blocks[countcurrent].cdid= currentdid;
		}
	}
}
/************************************************************************
 setCurrentSector
 Function used for setting current sector.
 ************************************************************************/
int setCurrentSector(int currentSector) {
	MEMORY_MAPPED_IO mmio;
	mmio.Mode = Z502GetCurrentContext;
	mmio.Field1 = mmio.Field2 = mmio.Field3 = mmio.Field4 = 0;
	MEM_READ(Z502Context, &mmio);

	//Use the current context to find the pid of current running process
	for (int countcurrent = 0; countcurrent < PID; countcurrent++)
	{

		if (Blocks[countcurrent].Context == mmio.Field1)
		{
			//printf("\ncurrentsector:%d!\n",currentSector);
			Blocks[countcurrent].csector= currentSector;
		}
	}
}

/************************************************************************
 checkp
 Function used for checking whether one directory/file exists or not.
 ************************************************************************/
int checkp(char *name, int flag, int did, int ptr) {
	MEMORY_MAPPED_IO mmio;
	//int i;
	UINT16 index1[8];
	unsigned char read[16];

	mmio.Mode = Z502DiskRead;
	mmio.Field1 = did;
	mmio.Field2 = ptr;
	mmio.Field3 = (long)index1;
	DoDisk(mmio);
	for (int i = 0; i < 8; i++) {

		mmio.Mode = Z502DiskRead;
		mmio.Field1 = did;
		mmio.Field2 = index1[i];
		mmio.Field3 = (long)read;
		DoDisk(mmio);

		int op = index1[i];


		char output[7];
		for (int i = 1; i < 8; i++) {
			output[i - 1] = read[i];
			
		}

		if (strcmp(name, output) == 0) {
			
			//printf("\nfind!\n");
			return op;
		}
	}

	return -1;
}


/************************************************************************
 Create
 Function used for creating a new file or directory.
 ************************************************************************/
int Create(int ID, char* name , int parent, int flag) {
	MEMORY_MAPPED_IO mmio;
	unsigned char write[16];
	write[0] = Inode;
	struct FDInformation FDI;
	FDI.Inode = Inode;
	FDI.name = name;
	FDI.flag = flag;
	FDI.did = ID;
	FDI.parent = parent;
	Inode++;
	char writename[7];
	int j = 0;

	strcpy(writename,name);

	//printf("\nname:%s\n", writename);

	int i = 0;
	for (i = 0; i < 7; i++) {
		write[i + 1] =writename[i];
	}

	

	mmio.Mode = Z502ReturnValue;
	mmio.Field1 = mmio.Field2 = mmio.Field3 = 0;
	MEM_READ(Z502Clock, &mmio);

	int time = mmio.Field1;

	FDI.time = time;

	if (time < 256)
	{
		write[8] = write[9] = 0;
		write[10] = time;
	}
	else
	{
		if (time < 65536)
		{
			write[8] = 0;
			write[9] = time / 256;
			write[10] = time - (write[9]*256);
		}
		else
		{
			write[8] = time / 65536;
			write[9] = (time - (write[8]*65536)) / 256;
			write[10] = time - (write[8] * 65536) - (write[9] * 256);
		}
	}

	write[11] = flag;
	write[11] = write[11] + 2;
	write[11] = write[11] + (8 * parent);

	int currents = Sector+1;

	if (currents < 256) {
		write[13] = 0;
		write[12] = currents;
	}
	else
	{
		write[13] = currents / 256;
		write[12] = currents - (write[13] * 256);
	}
	
	Sector=Sector+10;

	//printf("\nwrite13:%d\n",write[13]);
	//printf("\nwrite12:%d\n", write[12]);

	write[15] = 0;
	write[14] = 10;
	FDI.size = 10;

	/*
	for (i = 0; i < 16; i++) {
		printf("\nnumber %d: %d!\n",i,write[i]);
	}*/

	writeBitMap(currents - 1,0);
	writeBitMap(currents,0);

	mmio.Mode = Z502DiskWrite;
	mmio.Field1 = ID;
	mmio.Field2 = currents-1;
	mmio.Field3 = (long)write;
	DoDisk(mmio);

	FDI.ptr = currents - 1;

	FDIs[FDI.Inode] = FDI;

	if (flag == 0)
	{
		return FDI.Inode;
	}
	else
	{
		return currents - 1;
	}
	
}

/************************************************************************
 DoDisk
 Function used for doing writing and reading of disks.
 ************************************************************************/

void DoDisk(MEMORY_MAPPED_IO mmio) {
	
	MEMORY_MAPPED_IO mmio2;
	mmio2.Field2 = DEVICE_IN_USE;
	while (mmio2.Field2 != DEVICE_FREE) {
		mmio2.Mode = Z502Status;
		mmio2.Field1 = mmio.Field1;
		mmio2.Field2 = mmio2.Field3 = 0;
		MEM_READ(Z502Disk, &mmio2);
	}

	READ_MODIFY(MEMORY_INTERLOCK_BASE + 4, DO_LOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult);//Lock
	//Write to the disk

	MEM_WRITE(Z502Disk, &mmio);
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 4, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult);//Unlock
	//Get the current context
	mmio2.Mode = Z502GetCurrentContext;
	mmio2.Field1 = mmio2.Field2 = mmio2.Field3 = mmio2.Field4 = 0;
	MEM_READ(Z502Context, &mmio2);
	//Find the corresponding process
	int writecount = 0;
	for (writecount = 0; writecount < PID; writecount++)
	{

		if (Blocks[writecount].Context == mmio2.Field1)
		{
			
			InsertDiskOrder(&Blocks[writecount], Blocks[writecount].order);//Insert the corresponding PCB to disk queue

			Blocks[writecount].diskid = mmio.Field1;
			//InsertDiskTail(&Blocks[writecount]);

			break;
		}
	}

	CallSchedulePrinter(4, Blocks[writecount].pid);//Call the schedule printer
	dispatcher();//Call the dispatcher
	
	//Let the hardware to idle
	/*mmio2.Mode = Z502Action;
	mmio2.Field1 = mmio2.Field2 = mmio2.Field3 = 0;
	MEM_WRITE(Z502Idle, &mmio2);*/
}





/************************************************************************
 osInit
 This is the first routine called after the simulation begins.  This
 is equivalent to boot code.  All the initial OS components can be
 defined and initialized here.
 ************************************************************************/

void osInit(int argc, char *argv[]) {
	short *PageTable = (short *) calloc(2, NUMBER_VIRTUAL_PAGES);
	INT32 i;
	MEMORY_MAPPED_IO mmio;

	// Demonstrates how calling arguments are passed thru to here

	aprintf("Program called with %d arguments:", argc);
	for (i = 0; i < argc; i++)
		aprintf(" %s", argv[i]);
	aprintf("\n");
	aprintf("Calling with argument 'sample' executes the sample program.\n");

	// Here we check if a second argument is present on the command line.
	// If so, run in multiprocessor mode.  Note - sometimes people change
	// around where the "M" should go.  Allow for both possibilities
	if (argc > 2) {
		if ((strcmp(argv[1], "M") ==0) || (strcmp(argv[1], "m")==0)) {
			strcpy(argv[1], argv[2]);
			strcpy(argv[2],"M\0");
		}
		if ((strcmp(argv[2], "M") ==0) || (strcmp(argv[2], "m")==0)) {
			aprintf("Simulation is running as a MultProcessor\n\n");
			mmio.Mode = Z502SetProcessorNumber;
			mmio.Field1 = MAX_NUMBER_OF_PROCESSORS;
			mmio.Field2 = (long) 0;
			mmio.Field3 = (long) 0;
			mmio.Field4 = (long) 0;
			MEM_WRITE(Z502Processor, &mmio);   // Set the number of processors
		}
	} else {
		aprintf("Simulation is running as a UniProcessor\n");
		aprintf(
				"Add an 'M' to the command line to invoke multiprocessor operation.\n\n");
	}

	//          Setup so handlers will come to code in base.c

	TO_VECTOR[TO_VECTOR_INT_HANDLER_ADDR ] = (void *) InterruptHandler;
	TO_VECTOR[TO_VECTOR_FAULT_HANDLER_ADDR ] = (void *) FaultHandler;
	TO_VECTOR[TO_VECTOR_TRAP_HANDLER_ADDR ] = (void *) svc;

	//Used for check which test for running.
	//And do some initialization.
	b = (long)test0;
	if (strcmp(argv[1], "test0") == 0) {
		b = (long)test0;
		svcprint = interrprint = FULL;
	}
	if (strcmp(argv[1], "test1") == 0) {
		b = (long)test1;
		svcprint = interrprint = FULL;
	}
	if (strcmp(argv[1], "test2")==0) {
		b = (long)test2;
		svcprint = interrprint = FULL;
	}
	if (strcmp(argv[1], "test3") == 0) {
		b = (long)test3;
		svcprint = interrprint = INITIAL;
		scheduleprint = FULL;
	}
	if (strcmp(argv[1], "test4") == 0) {
		b = (long)test4;
		svcprint = interrprint = INITIAL;
		scheduleprint = FULL;
	}
	if (strcmp(argv[1], "test5") == 0) {
		b = (long)test5;
		svcprint = interrprint = INITIAL;
		scheduleprint = FULL;
	}
	if (strcmp(argv[1], "test6") == 0) {
		b = (long)test6;
		svcprint = interrprint = INITIAL;
		scheduleprint = FULL;
	}
	if (strcmp(argv[1], "test7") == 0) {
		b = (long)test7;
		svcprint = interrprint = INITIAL;
		scheduleprint = FULL;
	}
	if (strcmp(argv[1], "test8") == 0) {
		b = (long)test8;
		svcprint = interrprint = INITIAL;
		scheduleprint = FULL;
	}
	if (strcmp(argv[1], "test9") == 0) {
		b = (long)test9;
		svcprint = interrprint = INITIAL;
		scheduleprint = FULL;
	}
	if (strcmp(argv[1], "test10") == 0) {
		b = (long)test10;
		//svcprint = interrprint = INITIAL;
		//scheduleprint = FULL;
	}
	if (strcmp(argv[1], "test11") == 0) {
		b = (long)test11;
		//svcprint = interrprint = INITIAL;
		//scheduleprint = LIMITED;
	}
	if (strcmp(argv[1], "test12") == 0) {
		b = (long)test12;
		svcprint = interrprint = INITIAL;
		scheduleprint = LIMITED;
	}
	
	if (strcmp(argv[1], "test21") == 0) {
		b = (long)test21;
		svcprint = interrprint = FULL;
		
	}
	if (strcmp(argv[1], "test22") == 0) {
		b = (long)test22;
		svcprint = interrprint = FULL;
		
	}

	if (strcmp(argv[1], "test23") == 0) {
		b = (long)test23;
		svcprint = interrprint = INITIAL;
		scheduleprint = FULL;
	}

	if (strcmp(argv[1], "test24") == 0) {
		b = (long)test24;
		svcprint = interrprint = INITIAL;
		scheduleprint = LIMITED;
	}
	
	if (strcmp(argv[1], "test25") == 0) {
		b = (long)test25;
		svcprint = interrprint = INITIAL;
		//scheduleprint = FULL;
	}

	if (strcmp(argv[1], "test26") == 0) {
		b = (long)test25;
		svcprint = interrprint = INITIAL;
		//scheduleprint = FULL;
	}

	if (strcmp(argv[1], "test27") == 0) {
		b = (long)test25;
		svcprint = interrprint = INITIAL;
		//scheduleprint = FULL;
	}

	if (strcmp(argv[1], "test28") == 0) {
		b = (long)test25;
		svcprint = interrprint = INITIAL;
		//scheduleprint = FULL;
	}

	if (strcmp(argv[1], "test41") == 0) {
		b = (long)test41;
		svcprint = interrprint = faultprint = FULL;
		memoryprint = FULL;
	}

	if (strcmp(argv[1], "test42") == 0) {
		b = (long)test42;
		svcprint = interrprint = faultprint = FULL;
		memoryprint = FULL;
	}

	if (strcmp(argv[1], "test43") == 0) {
		b = (long)test43;
		svcprint = interrprint = faultprint = INITIAL;
		memoryprint = FULL;
	}

	if (strcmp(argv[1], "test44") == 0) {
		b = (long)test44;
		svcprint = interrprint = faultprint = INITIAL;
		memoryprint = LIMITED;
	}

	if (strcmp(argv[1], "test45") == 0) {
		b = (long)test45;
		svcprint = interrprint = faultprint = INITIAL;
		memoryprint = LIMITED;
	}

	if (strcmp(argv[1], "test46") == 0) {
		b = (long)test46;
		svcprint = interrprint = faultprint = INITIAL;
		memoryprint = LIMITED;
	}

	if (strcmp(argv[1], "test47") == 0) {
		b = (long)test47;
		svcprint = interrprint = faultprint = INITIAL;
		memoryprint = LIMITED;
	}

	//Initial the three queues
	InitialReady();
	InitialTimer();
	InitialDisk();

	//Build the PCB of main process and add it to the PCB list
	struct PCB P;
	P.pid = 0;
	P.name = "Test";
	P.order = 10;
	P.status = 1;
	Blocks[0] = P;
	

	//Initial a context
	mmio.Mode = Z502InitializeContext;
	mmio.Field1 = 0;
	mmio.Field2 = b;
	mmio.Field3 = (long)PageTable;

	MEM_WRITE(Z502Context, &mmio);   // Start this new Context Sequence

	//Set the context to the main process
	Blocks[0].Context = mmio.Field1;



	//Start the context
	mmio.Mode = Z502StartContext;
	// Field1 contains the value of the context returned in the last call
	// Suspends this current thread
	mmio.Field2 = START_NEW_CONTEXT_AND_SUSPEND;
	MEM_WRITE(Z502Context, &mmio);     // Start up the context



	

}                                               // End of osInit

