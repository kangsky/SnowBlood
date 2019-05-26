# TODO List
	1. Memory Management
		a. Mode 1 : Pre-allocate a set of memory block with fixed size.  
		b. Mode 2 : Dynamically maintain the heap with double linked list
			- (Done) no alignment, validation/debugging
				- bug in free function, need to update blk_list if head block is updated
			- (Done)alignment
			- (Done) Remove addition malloc/free for double linked list. Add memory block header
			- Reduce block header size for memory optimization
	2. Application (Voice Recognition)
		a. Activate microphone and record voice, save it in file system
		b. Playback the voice from file system
		c. Voice to text conversion
		b. Bsaed on text keywords, reacts according, e.g. create a folder, play music etc.
