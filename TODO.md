TODO List
	1. Memory Management
		a. Mode 1 : Pre-allocate a set of memory block with fixed size.  
		b. Mode 2 : Dynamically maintain the heap with double linked list
			- (Done) no alignment, validation/debugging
				- bug in free function, need to update blk_list if head block is updated
			- (Done)alignment
			- (Done) Remove addition malloc/free for double linked list. Add memory block header
			- Reduce block header size for memory optimization
	2. Application ( Send multiple files over TCP in parallel )
			- (Done) TCP Connection client/server
				1) (Done) Debugging live chatting
				2) (Done) Makefiles for multiple target executable files
			- (Done) Send one file over TCP
			- Send multiple files in parallel
				1) (Done) Multi-threading for client/server
				2) (Done) Directory iteration
				3) (Done) Producer to read all files / consumer for all clients to read one of them / maintain one buffer
				4) (Done) Message header for file name transmission
				5) (Done) TCP packet frame transmission with header
				6) (Done) Debugging intergration bugs! Last step to complete TODO list here!
				7) (Done) Code Polish + Better debugging framework

DONE!!!

	Next Step:
		Code refactoring
