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
			- TCP Connection client/server
			- Send one file over TCP
			- Send multiple files in parallel
