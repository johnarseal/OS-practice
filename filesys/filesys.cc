// filesys.cc 
//	Routines to manage the overall operation of the file system.
//	Implements routines to map from textual file names to files.
//
//	Each file in the file system has:
//	   A file header, stored in a sector on disk 
//		(the size of the file header data structure is arranged
//		to be precisely the size of 1 disk sector)
//	   A number of data blocks
//	   An entry in the file system directory
//
// 	The file system consists of several data structures:
//	   A bitmap of free disk sectors (cf. bitmap.h)
//	   A directory of file names and file headers
//
//      Both the bitmap and the directory are represented as normal
//	files.  Their file headers are located in specific sectors
//	(sector 0 and sector 1), so that the file system can find them 
//	on bootup.
//
//	The file system assumes that the bitmap and directory files are
//	kept "open" continuously while Nachos is running.
//
//	For those operations (such as Create, Remove) that modify the
//	directory and/or bitmap, if the operation succeeds, the changes
//	are written immediately back to disk (the two files are kept
//	open during all this time).  If the operation fails, and we have
//	modified part of the directory and/or bitmap, we simply discard
//	the changed version, without writing it back to disk.
//
// 	Our implementation at this point has the following restrictions:
//
//	   there is no synchronization for concurrent accesses
//	   files have a fixed size, set when the file is created
//	   files cannot be bigger than about 3KB in size
//	   there is no hierarchical directory structure, and only a limited
//	     number of files can be added to the system
//	   there is no attempt to make the system robust to failures
//	    (if Nachos exits in the middle of an operation that modifies
//	    the file system, it may corrupt the disk)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "disk.h"
#include "bitmap.h"
#include "directory.h"
#include "filehdr.h"
#include "filesys.h"
#include<time.h>
// Sectors containing the file headers for the bitmap of free sectors,
// and the directory of files.  These file headers are placed in well-known 
// sectors, so that they can be located on boot-up.
#define FreeMapSector 		0
#define DirectorySector 	1

// Initial file sizes for the bitmap and directory; until the file system
// supports extensible files, the directory size sets the maximum number 
// of files that can be loaded onto the disk.
#define FreeMapFileSize 	(NumSectors / BitsInByte)
#define NumDirEntries 		10
#define DirectoryFileSize 	(sizeof(DirectoryEntry) * NumDirEntries)

//----------------------------------------------------------------------
// FileSystem::FileSystem
// 	Initialize the file system.  If format = TRUE, the disk has
//	nothing on it, and we need to initialize the disk to contain
//	an empty directory, and a bitmap of free sectors (with almost but
//	not all of the sectors marked as free).  
//
//	If format = FALSE, we just have to open the files
//	representing the bitmap and the directory.
//
//	"format" -- should we initialize the disk?
//----------------------------------------------------------------------

FileSystem::FileSystem(bool format)
{ 
    DEBUG('f', "Initializing the file system.\n");
    if (format) {
		BitMap *freeMap = new BitMap(NumSectors);
		Directory *directory = new Directory(NumDirEntries);
		FileHeader *mapHdr = new FileHeader;
		FileHeader *dirHdr = new FileHeader;

			DEBUG('f', "Formatting the file system.\n");

		// First, allocate space for FileHeaders for the directory and bitmap
		// (make sure no one else grabs these!)
		freeMap->Mark(FreeMapSector);	    
		freeMap->Mark(DirectorySector);

		// Second, allocate space for the data blocks containing the contents
		// of the directory and bitmap files.  There better be enough space!

		ASSERT(mapHdr->Allocate(freeMap, FreeMapFileSize));
		ASSERT(dirHdr->Allocate(freeMap, DirectoryFileSize));

		// Flush the bitmap and directory FileHeaders back to disk
		// We need to do this before we can "Open" the file, since open
		// reads the file header off of disk (and currently the disk has garbage
		// on it!).

			DEBUG('f', "Writing headers back to disk.\n");
		mapHdr->WriteBack(FreeMapSector);    
		dirHdr->WriteBack(DirectorySector);

		// OK to open the bitmap and directory files now
		// The file system operations assume these two files are left open
		// while Nachos is running.

		freeMapFile = new OpenFile(FreeMapSector);
		directoryFile = new OpenFile(DirectorySector);
		 
		// Once we have the files "open", we can write the initial version
		// of each file back to disk.  The directory at this point is completely
		// empty; but the bitmap has been changed to reflect the fact that
		// sectors on the disk have been allocated for the file headers and
		// to hold the file data for the directory and bitmap.

			DEBUG('f', "Writing bitmap and directory back to disk.\n");
		freeMap->WriteBack(freeMapFile);	 // flush changes to disk
		directory->WriteBack(directoryFile);

		if (DebugIsEnabled('f')) {
			freeMap->Print();
			directory->Print();

			delete freeMap; 
			delete directory; 
			delete mapHdr; 
			delete dirHdr;
		}
    } else {
    // if we are not formatting the disk, just open the files representing
    // the bitmap and directory; these are left open while Nachos is running
        freeMapFile = new OpenFile(FreeMapSector);
        directoryFile = new OpenFile(DirectorySector);
    }
}

// return the sector of the file with filename name
int
FileSystem::DirTranslate(char *name){
	
	// the file name must start with '/'
	ASSERT(name[0] == '/');
	
	int nameLen = strlen(name);
	int j = 0;
	int sector;
	char tmpPath[255];
	Directory *directory = new Directory(NumDirEntries);
	OpenFile *dirFile = directoryFile;
	
	for(int i = 1; i < nameLen; i++){
		tmpPath[j++] = name[i];
		// encounter an '/' or it is the last token
		if(name[i] == '/' || i == nameLen - 1){
			tmpPath[j] = 0;
			directory->FetchFrom(dirFile);
			if(dirFile != directoryFile){
				delete dirFile;
			}
			sector = directory->Find(tmpPath);
			//if the file doesn't exist or it is the dir we are looking for
			if(sector == -1 || i == nameLen - 1){
				delete directory;
				return sector;		
			}
			else{ 
				dirFile = new OpenFile(sector);
			}
			j = 0;
		}
	}
}

// add a single dir to the
int
FileSystem::AllocateDir(BitMap *freeMap){
	FileHeader *dirHdr = new FileHeader;
    int hdrSector = freeMap->Find();	// find a sector to hold the file header
	if (hdrSector == -1){
		return -1;		// no free block for file header 
	}
	if(!dirHdr->Allocate(freeMap, DirectoryFileSize)){
		return -1;
	}
	dirHdr->createTime = time(NULL);
	//write back the diretory header
	dirHdr->WriteBack(hdrSector);
	//write back the directory
	OpenFile *dirFile = new OpenFile(hdrSector);
	Directory *directory = new Directory(NumDirEntries);
	directory->WriteBack(dirFile);
	delete dirHdr;
	delete dirFile;
	delete directory;
	return hdrSector;
}
//----------------------------------------------------------------------
// FileSystem::Create
// 	Create a file in the Nachos file system (similar to UNIX create).
//	Since we can't increase the size of files dynamically, we have
//	to give Create the initial size of the file.
//
//	The steps to create a file are:
//	  Make sure the file doesn't already exist
//        Allocate a sector for the file header
// 	  Allocate space on disk for the data blocks for the file
//	  Add the name to the directory
//	  Store the new file header on disk 
//	  Flush the changes to the bitmap and the directory back to disk
//
//	Return TRUE if everything goes ok, otherwise, return FALSE.
//
// 	Create fails if:
//   		file is already in directory
//	 	no free space for file header
//	 	no free entry for file in directory
//	 	no free space for data blocks for the file 
//
// 	Note that this implementation assumes there is no concurrent access
//	to the file system!
//
//	"name" -- name of file to be created
//	"initialSize" -- size of file to be created
//	type=1:	creating dir
//		=0:	creating file
//----------------------------------------------------------------------

bool
FileSystem::Create(char *name, int initialSize,int type)
{
    ASSERT(name[0] == '/');
	int nameLen = strlen(name);
	if(type == 1){
		ASSERT(name[nameLen-1] == '/');
		initialSize = DirectoryFileSize;
	}
	
	//ptr that might need to be deleted
	Directory *directory;
    BitMap *freeMap;
    FileHeader *hdr;
	OpenFile *dirFile = directoryFile;
	
    int sector,dirHdrSector,fileHdrSector;
	int j = 0;
    bool success;
	char tmpPath[255];
	
    DEBUG('f', "Creating file %s, size %d\n", name, initialSize);

    directory = new Directory(NumDirEntries);
    directory->FetchFrom(dirFile);
	
    if (DirTranslate(name) != -1){
		success = FALSE;			// file is already in directory
	}
    else {
        freeMap = new BitMap(NumSectors);
        freeMap->FetchFrom(freeMapFile);
		//try add it to the directory and create it
		for(int i = 1; i < nameLen; i++){
			tmpPath[j++] = name[i];
			// encounter an '/' or the last character
			if(name[i] == '/' || i == nameLen - 1){
				tmpPath[j] = 0;
				directory->FetchFrom(dirFile);
				sector = directory->Find(tmpPath);
				//if the file/diretory doesn't exist
				if(sector == -1){
					//if it is a directory and it doesn't exist
					if(name[i] == '/'){
						// allocate the directory
						if((dirHdrSector = AllocateDir(freeMap)) < 0){
							success = FALSE;
							break;
						}
						// add the diretocy to the current diretory
						if (!directory->Add(tmpPath,dirHdrSector,1)){
							success = FALSE;
							break;
						}
						directory->WriteBack(dirFile);			
						if(dirFile != directoryFile){
							delete dirFile;
						}
						dirFile = new OpenFile(dirHdrSector);
						// if it is the directory that is required to be created, then it is already created above
						if(i == nameLen - 1){
							freeMap->WriteBack(freeMapFile);
							delete dirFile;
							success = TRUE;			//success creating a directory
						}
					}
					//if it is a file and it doesn't exist, then it must also be the last char
					else{
						fileHdrSector = freeMap->Find();	// find a sector to hold the file header
						if (fileHdrSector == -1){
							success = FALSE;		// no free block for file header 
						}
						else{
							hdr = new FileHeader;
							if (!hdr->Allocate(freeMap, initialSize)){
								success = FALSE;	// no space on disk for data
							}
							else {
								// add the file to the current diretory
								if (!directory->Add(tmpPath,fileHdrSector,type)){
									success = FALSE;
								}
								else{
									success = TRUE; 		//success creating a file
									hdr->createTime = time(NULL);
									// everything worked, flush all changes back to disk
									hdr->WriteBack(fileHdrSector); 		
									directory->WriteBack(dirFile);
									if(dirFile != directoryFile){
										delete dirFile;
									}
									freeMap->WriteBack(freeMapFile);
								}
							}
							delete hdr;
							success = TRUE;
						}
					}
				}
				// if it exists, then it can not be a file, else we are creating a file that already exists!
				else{
					if(dirFile != directoryFile){
						delete dirFile;
					}			
					dirFile = new OpenFile(sector);
				}
				j = 0;
			}
		}
		delete freeMap;
    }
	delete directory;
	return success;
}

//----------------------------------------------------------------------
// FileSystem::Open
// 	Open a file for reading and writing.  
//	To open a file:
//	  Find the location of the file's header, using the directory 
//	  Bring the header into memory
//
//	"name" -- the text name of the file to be opened
//----------------------------------------------------------------------

OpenFile *
FileSystem::Open(char *name)
{ 
    Directory *directory = new Directory(NumDirEntries);
    OpenFile *openFile = NULL;
    int sector;

    DEBUG('f', "Opening file %s\n", name);
    directory->FetchFrom(directoryFile);
    sector = DirTranslate(name); 
    if (sector >= 0){
		openFile = new OpenFile(sector);	// name was found in directory
	}
    delete directory;
    return openFile;				// return NULL if not found
}

//----------------------------------------------------------------------
// FileSystem::Remove
// 	Delete a file from the file system.  This requires:
//	    Remove it from the directory
//	    Delete the space for its header
//	    Delete the space for its data blocks
//	    Write changes to directory, bitmap back to disk
//
//	Return TRUE if the file was deleted, FALSE if the file wasn't
//	in the file system.
//
//	"name" -- the text name of the file to be removed
//----------------------------------------------------------------------

bool
FileSystem::Remove(char *name)
{ 
    Directory *directory;
    BitMap *freeMap;
    FileHeader *fileHdr;
    int sector;
    
    directory = new Directory(NumDirEntries);
    directory->FetchFrom(directoryFile);
    sector = directory->Find(name);
    if (sector == -1) {
       delete directory;
       return FALSE;			 // file not found 
    }
    fileHdr = new FileHeader;
    fileHdr->FetchFrom(sector);

    freeMap = new BitMap(NumSectors);
    freeMap->FetchFrom(freeMapFile);

    fileHdr->Deallocate(freeMap);  		// remove data blocks
    freeMap->Clear(sector);			// remove header block
    directory->Remove(name);

    freeMap->WriteBack(freeMapFile);		// flush to disk
    directory->WriteBack(directoryFile);        // flush to disk
    delete fileHdr;
    delete directory;
    delete freeMap;
    return TRUE;
} 

//----------------------------------------------------------------------
// FileSystem::List
// 	List all the files in the file system directory.
//----------------------------------------------------------------------

void
FileSystem::List()
{
    Directory *directory = new Directory(NumDirEntries);

    directory->FetchFrom(directoryFile);
    directory->List();
    delete directory;
}

//----------------------------------------------------------------------
// FileSystem::Print
// 	Print everything about the file system:
//	  the contents of the bitmap
//	  the contents of the directory
//	  for each file in the directory,
//	      the contents of the file header
//	      the data in the file
//----------------------------------------------------------------------

void
FileSystem::Print()
{
    FileHeader *bitHdr = new FileHeader;
    FileHeader *dirHdr = new FileHeader;
    BitMap *freeMap = new BitMap(NumSectors);
    Directory *directory = new Directory(NumDirEntries);

    printf("Bit map file header:\n");
    bitHdr->FetchFrom(FreeMapSector);
    bitHdr->Print();

    printf("Directory file header:\n");
    dirHdr->FetchFrom(DirectorySector);
    dirHdr->Print();

    freeMap->FetchFrom(freeMapFile);
    freeMap->Print();

    directory->FetchFrom(directoryFile);
    directory->Print();

    delete bitHdr;
    delete dirHdr;
    delete freeMap;
    delete directory;
} 
