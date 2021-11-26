/* ECMA and OSTA structures as documented in ECMA-119, ECMA-167, ECMA-168, and UDF standards v1.02 through v2.60.
 * Note that these structures require that your compiler support packing the bytes together, NO STRUCTURE MEMBER ALIGNMENT! */
#ifndef _UDF_H
#define _UDF_H

#include "bytes.h"

/* this allows porting to other compilers easily that don't recognize GCC __attribute__ ((packed)) */
#define PACKED __attribute__ ((packed))

/* typedefs to align types with ECMA standards */
typedef unsigned char UDF_Uint8;
typedef signed char UDF_Int8;
typedef unsigned short UDF_Uint16;
typedef signed short UDF_Int16;
typedef unsigned int UDF_Uint32;
typedef signed int UDF_Int32;
typedef unsigned long long UDF_Uint64;
typedef signed long long UDF_Int64;

/* CD-ROM volume descriptor (ECMA-119 1/8.1)
 * NOTE: Standard specifies offsets 1-based */
typedef struct {						// byte offset    BP
	UDF_Uint8	VolumeDescriptorType;			// +0             1
		// 0 = boot record
		// 1 = primary volume
		// 2 = supplementary volume
		// 3 = volume partition
		// 255 = terminator
	UDF_Uint8	StandardIdentifier[5];			// +1             2...6
		// usually "CD001" for ISO9660 compat.
	UDF_Uint8	VolumeDescriptorVersion;		// +6             7
	UDF_Uint8	descriptor[2041];			// +7             8...2048
} ISO9660_volumedescriptor  PACKED;

#define	VOLUME_CDROM		"CD001"
#define VOLUME_BEA01		"BEA01"
#define VOLUME_BOOT2		"BOOT2"
#define VOLUME_NSR02		"NSR02"
#define VOLUME_NSR03		"NSR03"
#define VOLUME_TEA01		"TEA01"

/* charspec (ECMA-167 1/7.2.1) */
typedef struct {
	UDF_Uint8	CharacterSetType;
	UDF_Uint8	CharacterSetInformation[63];
} UDF_charspec  PACKED;

/* dstring (ECMA-167 1/7.2.12) */
typedef UDF_Uint8 UDF_dstring;

#define UDF_dstring_strncpyne(d,sz,str) { \
	int l = strlen(str); \
	if (l > ((sz)-1)) l = (sz)-1; \
	if (l == 0) { \
		d[0] = 0; \
	} \
	else { \
		d[0] = 8;	/* OSTA Compressed Unicode 8-bit */ \
	} \
	memset(d+1,0,(sz)-1); \
	if (l > 0) memcpy(d+1,str,l); \
}

#define UDF_dstring_strncpy(d,sz,str) { \
	int l = strlen(str); \
	if (l > ((sz)-2)) l = (sz)-2; \
	if (l == 0) { \
		d[0] = 0; \
	} \
	else { \
		d[0] = 8;	/* OSTA Compressed Unicode 8-bit */ \
	} \
	memset(d+1,0,(sz)-1); \
	if (l > 0) memcpy(d+1,str,l); \
	if (l > 0) d[sz-1] = l+1; \
}

/* timestamp (ECMA-167 1/7.3) */
typedef struct {
	UDF_Uint16	TypeAndTimeZone;		// +0
		// bits 15-12:
		//    0 = Coordinated Universal Time
		//    1 = local time
		//    2 = subject between originator and recipient of this medium
		// bits 11-0: (signed integer)
		//    -2047 = undefiend
		//    -1440...1440 = offset in minutes from UTC
	UDF_Int16	Year;				// +2
	UDF_Uint8	Month;				// +4
	UDF_Uint8	Day;				// +5
	UDF_Uint8	Hour;				// +6
	UDF_Uint8	Minute;				// +7
	UDF_Uint8	Second;				// +8
	UDF_Uint8	Centiseconds;			// +9
	UDF_Uint8	HundredsOfMicroseconds;		// +10
	UDF_Uint8	Microseconds;			// +11
} UDF_timestamp  PACKED;

/* regid (Entity Identifier) (ECMA-167 1/7.4) */
typedef struct {
	UDF_Uint8	Flags;				// +0
		// bit 0: dirty
		// bit 1: protected
	UDF_Uint8	Identifier[23];			// +1
	UDF_Uint8	IdentifierSuffix[8];		// +24
} UDF_regid  PACKED;

#define SET_UDF_regid(tag,flag,id,idsuf) { \
	memset(&(tag),0,sizeof(UDF_regid)); \
	(tag).Flags = flag; \
	strncpy((char*)((tag).Identifier),(const char*)(id),23); \
	strncpy((char*)((tag).IdentifierSuffix),(const char*)(idsuf),8); \
}

/* Beginning Extended Area (ECMA-167 2/9.2) */
typedef ISO9660_volumedescriptor UDF_volumedescriptor_BEA;	// same as ISO9660, identifier "BEA01"

/* Terminating Extended Area (ECMA-167 2/9.3) */
typedef ISO9660_volumedescriptor UDF_volumedescriptor_TEA;	// identifier "TEA01"

/* Boot Descriptor (ECMA-167 2/9.4) */
// TODO: This is not immediately relevant, people use El Torito anyway

/* extent_ad (ECMA-167 3/7.1) */
typedef struct {
	UDF_Uint32	length;		// always less than 2^30 because upper bits are used differently
	UDF_Uint32	location;	// sector number, or 0 if no extent
} UDF_extent_ad  PACKED;

#define SET_UDF_extent_ad(a,x,y) { \
	LSETDWORD(&((a).length),(x)); \
	LSETDWORD(&((a).location),(y)); \
}

/* tag (ECMA-167 3/7.2) */
typedef struct {
	UDF_Uint16	TagIdentifier;			// +0
	UDF_Uint16	DescriptorVersion;		// +2	(either 2 or 3 depending on which ECMA-167 was implemented)
	UDF_Uint8	TagChecksum;			// +4	(sum of bytes 0...3 and 5...15 of this tag modulo 256)
	UDF_Uint8	reserved;			// +5
	UDF_Uint16	TagSerialNumber;		// +6
	UDF_Uint16	DescriptorCRC;			// +8	(CRC-ITU-T x^16 + x^12 + x^5 + 1 16-bit, or 0 for lazy implementations)
	UDF_Uint16	DescriptorCRCLength;		// +10	(how many bytes used in CRC)
	UDF_Uint32	TagLocation;			// +12	(the number of the sector holding this tag)
} UDF_tag  PACKED;					// +16

#define SET_UDF_tag(tag,id,location) { \
	LSETWORD(&((tag).DescriptorVersion),2); \
	LSETWORD(&((tag).TagIdentifier),id); \
	UDF_Uint32 v = location; \
	LSETDWORD(&((tag).TagLocation),v); \
	LSETWORD(&((tag).TagSerialNumber),1); \
}
#define UPDATE_UDF_tag(tag) { \
	unsigned char *x = (unsigned char*)(&(tag)); \
	unsigned char checksum = 0; \
	int i; \
	\
	for (i=0;i <= 3;i++) \
		checksum += x[i]; \
	for (i=5;i <= 15;i++) \
		checksum += x[i]; \
	(tag).TagChecksum = checksum; \
}
#define SET_UDF_tag_checksum(tag,len) { \
	unsigned short v = osta_cksum(((unsigned char*)(&(tag)))+16,len); \
	LSETWORD(&((tag).DescriptorCRC),v); \
	LSETWORD(&((tag).DescriptorCRCLength),len); \
	UPDATE_UDF_tag(tag); \
}

/* TagIdentifier values */
enum {
	UDFtag_PrimaryVolumeDescriptor=1,		// (see ECMA-167 3/10.1)
	UDFtag_AnchorVolumeDescriptor=2,		// (see ECMA-167 3/10.2)
	UDFtag_VolumeDescriptorPointer=3,		// (see ECMA-167 3/10.3)
	UDFtag_ImplementationUseVolumeDescriptor=4,	// (see ECMA-167 3/10.4)
	UDFtag_PartitionDescriptor=5,			// (see ECMA-167 3/10.5)
	UDFtag_LogicalVolumeDescriptor=6,		// (see ECMA-167 3/10.6)
	UDFtag_UnallocatedSpaceDescriptor=7,		// (see ECMA-167 3/10.8)
	UDFtag_TerminatingDescriptor=8,			// (see ECMA-167 3/10.9 and 4/14.2)
	UDFtag_LogicalVolumeIntegrityDescriptor=9,	// (see ECMA-167 3/10.10)

	UDFtag_FileSetDescriptor=256,			// (see ECMA-167 4/14.1)
	UDFtag_FileIdentifierDescriptor=257,		// (see ECMA-167 4/14.4)
	UDFtag_AllocationExtentDescriptor=258,		// (see ECMA-167 4/14.5)
	UDFtag_IndirectEntry=259,			// (see ECMA-167 4/14.7)
	UDFtag_TerminalEntry=260,			// (see ECMA-167 4/14.8)
	UDFtag_FileEntry=261,				// (see ECMA-167 4/14.9)
	UDFtag_ExtendedAttributeHeaderDescriptor=262,	// (see ECMA-167 4/14.10.1)
	UDFtag_UnallocatedSpaceEntry=263,		// (see ECMA-167 4/14.11)
	UDFtag_SpaceBitmapDescriptor=264,		// (see ECMA-167 4/14.12)
	UDFtag_PartitionIntegrityEntry=265,		// (see ECMA-167 4/14.13)
	UDFtag_ExtendedFileEntry=266,			// (see ECMA-167 4/14.17)
};

/* NSR descriptor */
typedef ISO9660_volumedescriptor UDF_volumedescriptor_NSR;	// identifier "NSR02" or "NSR03"

/* Primary Volume Descriptor (ECMA-167 3/10.1) */
typedef struct {
	UDF_tag		DescriptorTag;				// +0	(id = 1)
	UDF_Uint32	VolumeDescriptorSequenceNumber;		// +16
	UDF_Uint32	PrimaryVolumeDescriptorNumber;		// +20
	UDF_dstring	VolumeIdentifier[32];			// +24
	UDF_Uint16	VolumeSequenceNumber;			// +56
	UDF_Uint16	MaximumVolumeSequenceNumber;		// +58
	UDF_Uint16	InterchangeLevel;			// +60
	UDF_Uint16	MaximumInterchangeLevel;		// +62
	UDF_Uint32	CharacterSetList;			// +64
	UDF_Uint32	MaximumCharacterSetList;		// +68
	UDF_dstring	VolumeSetIdentifier[128];		// +72
	UDF_charspec	DescriptorCharacterSet;			// +200
	UDF_charspec	ExplanatoryCharacterSet;		// +264
	UDF_extent_ad	VolumeAbstract;				// +328
	UDF_extent_ad	VolumeCopyrightNotice;			// +336
	UDF_regid	ApplicationIdentifier;			// +344
	UDF_timestamp	RecordingDateAndTime;			// +376
	UDF_regid	ImplementationIdentifier;		// +388
	UDF_Uint8	ImplementationUse[64];			// +420
	UDF_Uint32	PredecessorVolumeDescriptor;		// +484
	UDF_Uint16	Flags;					// +488
		// bit 0: volume set identification (3/10.1.21)
	UDF_Uint8	reserved[22];				// +490
} UDF_tag_primary_volume_descriptor  PACKED;			// +512

/* Anchor Volume Descriptor Pointer (ECMA-167 3/10.2) */
typedef struct {
	UDF_tag		DescriptorTag;				// +0   (id = 2)
	UDF_extent_ad	MainVolumeDescriptorSequenceExtent;	// +16
	UDF_extent_ad	ReserveVolumeDescriptorSequenceExtent;	// +24
	UDF_Uint8	reserved[480];				// +32
} UDF_tag_anchor_volume_descriptor  PACKED;			// +512

/* Volume Descriptor Pointer (ECMA-167 3/10.3) */
typedef struct {
	UDF_tag		DescriptorTag;				// +0   (id = 3)
	UDF_Uint32	VolumeDescriptorSequenceNumber;		// +16
	UDF_extent_ad	NextVolumeDescriptorSequenceExtent;	// +20
	UDF_Uint8	reserved[484];				// +24
} UDF_tag_volume_descriptor_pointer  PACKED;			// +512

/* Implementation Use Volume Descriptor (ECMA-167 3/10.4) */
typedef struct {
	UDF_tag		DescriptorTag;				// +0   (id = 4)
	UDF_Uint32	VolumeDescriptorSequenceNumber;		// +16
	UDF_regid	ImplementationIdentifier;		// +20
	UDF_Uint8	reserved[460];				// +52
} UDF_tag_implementation_use_volume_descriptor  PACKED;		// +512

/* Partition Descriptor (ECMA-167 3/10.5) */
typedef struct {
	UDF_tag		DescriptorTag;				// +0   (id = 5)
	UDF_Uint32	VolumeDescriptorSequenceNumber;		// +16
	UDF_Uint16	PartitionFlags;				// +20
	UDF_Uint16	PartitionNumber;			// +22
	UDF_regid	PartitionContents;			// +24
		// +FDC01    see ECMA-107
		// +CD001    see ECMA-119 aka ISO 9660
		// +CDW02    see ECMA-168
		// +NSR03    see part 4 of ECMA-167
	UDF_Uint8	PartitionContentsUse[128];		// +56
	UDF_Uint32	AccessType;				// +184
		// 1 = read only
		// 2 = write once
		// 3 = rewriteable
		// 4 = overwriteable
	UDF_Uint32	PartitionStartingLocation;		// +188
	UDF_Uint32	PartitionLength;			// +192
	UDF_regid	ImplementationIdentifier;		// +196
	UDF_Uint8	ImplementationUse[128];			// +228
	UDF_Uint8	reserved[156];				// +356
} UDF_tag_partition_descriptor  PACKED;				// +512

/* Logical Volume Descriptor (ECMA-167 3/10.6) */
typedef struct {
	UDF_tag		DescriptorTag;				// +0   (id = 6)
	UDF_Uint32	VolumeDescriptorSequenceNumber;		// +16
	UDF_charspec	DescriptorCharacterSet;			// +20
	UDF_dstring	LogicalVolumeIdentifier[128];		// +84
	UDF_Uint32	LogicalBlockSize;			// +212
	UDF_regid	DomainIdentifier;			// +216
	UDF_Uint8	LogicalVolumeContentsUse[16];		// +248
	UDF_Uint32	MapTableLength;				// +264
	UDF_Uint32	NumberOfPartitionMaps;			// +268
	UDF_regid	ImplementationIdentifier;		// +272
	UDF_Uint8	ImplementationUse[128];			// +304
	UDF_extent_ad	IntegritySequenceExtent;		// +432
	UDF_Uint8	PartitionMaps[1];			// +440, length = MapTableLength
} UDF_tag_logical_volume_descriptor  PACKED;			// +440 + MapTableLength

/* partition map (generic) (ECMA-167 3/10.7.1) */
typedef struct {
	UDF_Uint8	PartitionMapType;			// +0
		// 0 = unspecified
		// 1 = type 1 (ECMA-167 3/10.7.2)
		// 2 = type 2 (ECMA-167 3/10.7.3)
	UDF_Uint8	PartitionMapLength;			// +1
	UDF_Uint8	PartitionMapping[1];			// +2, length = PartitionMapLength - 2
} UDF_partition_map_generic  PACKED;				// +PartitionMapLength

/* Type 1 partition map (ECMA-167 3/10.7.2) */
typedef struct {
	UDF_Uint8	PartitionMapType;			// +0 = 1
	UDF_Uint8	PartitionMapLength;			// +1 = 6
	UDF_Uint16	VolumeSequenceNumber;			// +2
	UDF_Uint16	PartitionNumber;			// +4
} UDF_partition_map_type1  PACKED;				// +6

/* Type 2 partition map (ECMA-167 3/10.7.3) */
typedef struct {
	UDF_Uint8	PartitionMapType;			// +0 = 2
	UDF_Uint8	PartitionMapLength;			// +1 = 64
	UDF_Uint8	PartitionIdentifier[62];		// +2
		// in other words, how this works is up to the recipient and sender. goodie...
} UDF_partition_map_type2  PACKED;				// +64

/* Unallocated Space Descriptor (ECMA-167 3/10.8) */
typedef struct {
	UDF_tag		DescriptorTag;				// +0
	UDF_Uint32	VolumeDescriptorSequenceNumber;		// +16
	UDF_Uint32	NumberOfAllocationDescriptors;		// +20
	UDF_extent_ad	AllocationDescriptors[1];		// +24, length = 8 * NumberOfAllocationDescriptors;
} UDF_tag_unallocated_space_descriptor  PACKED;			// +32 + (NumberOfAllocationDescriptors * 8)

/* TerminatorDescriptor (ECMA-167 3/10.9) */
typedef struct {
	UDF_tag		DescriptorTag;				// +0
	UDF_Uint8	reserved[496];				// +16
} UDF_tag_terminating_descriptor  PACKED;			// +512

/* LogicalVolumeIntegrityDescriptor (ECMA-167 3/10.10) */
typedef struct {
	UDF_tag		DescriptorTag;				// +0
	UDF_timestamp	RecordingDateAndTime;			// +16
	UDF_Uint32	IntegrityType;				// +28
	UDF_extent_ad	NextIntegrityExtent;			// +32
	UDF_Uint8	LogicalVolumeContentsUse[32];		// +40
	UDF_Uint32	NumberOfPartitions;			// +72
	UDF_Uint32	LengthOfImplementationUse;		// +76
// END OF FIXED STRUCTURE					   +80
//	UDF_Uint32	FreeSpaceTable[NumberOfPartitions];	// +80
//	UDF_Uint32	SizeTable[NumberOfPartitions];		// +80 + (NumberOfPartitions*4)
//	UDF_Uint8	ImplementationUse[LengthOfImplementationUse]; // +80 + (NumberOfPartitions*4*2)
} UDF_tag_logical_volume_integrity_descriptor  PACKED;

/* ECMA-167 levels of volume interchange:
 *
 * Level 1 restrictions:
 *  - Anchor Volume Descriptor is recorded at sector 256 and N-256
 *  - The Volume Descriptor Sequences pointed to by the AVD are the same regardless of which anchor (256 or N-256)
 *  - Logical Volume Descriptor contains only Type 1 partition maps
 *  - partitions specified in any Logical Volume Descriptor, catencated end to end, form a single extent
 *  - there is only one Primary Volume Descriptor
 *  - there is at most one Implementation Use Descriptor in the Main Volume Descriptor Sequence
 *  - there is at most one Partition Descriptor with any given Partition Number in the Main Volume Desc. Seq.
 *  - a volume set consists of only one volume
 *
 * Level 2 restrictions:
 *  - a volume set consists of only one volume
 *
 * Level 3 restrictions:
 *  - none
 */

/* lb_addr Recorded address (ECMA-167 4/7.1) */
typedef struct {
	UDF_Uint32	LogicalBlockNumber  PACKED;		// +0
	UDF_Uint16	PartitionReferenceNumber  PACKED;	// +4  numeric identification (see ECMA-167 4/3.1)
} UDF_lb_addr  PACKED;

/* long_ad (ECMA-167 4/14.14.2) */
typedef struct {
	UDF_Uint32	ExtentLength;				// +0
	UDF_lb_addr	ExtentLocation;				// +4
	UDF_Uint8	ImplementationUse[6];			// +10
} UDF_long_ad  PACKED;

/* File Set Descriptor (ECMA-167 4/14.1) */
typedef struct {
	UDF_tag		DescriptorTag;				// +0   (id = 256)
	UDF_timestamp	RecordingDateAndTime;			// +16
	UDF_Uint16	InterchangeLevel;			// +28
	UDF_Uint16	MaximumInterchangeLevel;		// +30
	UDF_Uint32	CharacterSetList;			// +32
	UDF_Uint32	MaximumCharacterSetList;		// +36
	UDF_Uint32	FileSetNumber;				// +40
	UDF_Uint32	FileSetDescriptorNumber;		// +44
	UDF_charspec	LogicalVolumeIdentifierCharacterSet;	// +48
	UDF_dstring	LogicalVolumeIdentifier[128];		// +112
	UDF_charspec	FileSetCharacterSet;			// +240
	UDF_dstring	FileSetIdentifier[32];			// +304
	UDF_dstring	CopyrightFileIdentifier[32];		// +336
	UDF_dstring	AbstractFileIdentifier[32];		// +368
	UDF_long_ad	RootDirectoryICB;			// +400
	UDF_regid	DomainIdentifier;			// +416
	UDF_long_ad	NextExtent;				// +448
	UDF_long_ad	SystemStreamDirectoryICB;		// +464
	UDF_Uint8	reserved[32];				// +480;
} UDF_tag_file_set_descriptor  PACKED;				// +512

/* short_ad short allocation descriptor (ECMA-167 4/14.14.1) */
typedef struct {
	UDF_Uint32	ExtentLength;				// +0
		// bits 30-31:
		//     0 = allocated/recorded
		//     1 = allocated/not recorded
		//     2 = not alloc/not rec
		//     3 = next extent of allocation descriptors (ECMA-167 4/12)
	UDF_Uint32	ExtentPosition;				// +4
} UDF_short_ad  PACKED;

/* Partition Header Descriptor (ECMA-167 4/14.3) */
// TODO: Like most descriptors this has a tag, right?
//       ECMA-167 seems to imply otherwise, which is confusing.
typedef struct {
	UDF_short_ad	UnallocatedSpaceTable;			// +0
	UDF_short_ad	UnallocatedSpaceBitmap;			// +8
	UDF_short_ad	PartitionIntegrityTable;		// +16
	UDF_short_ad	FreedSpaceTable;			// +24
	UDF_short_ad	FreedSpaceBitmap;			// +32
	UDF_Uint8	reserved[88];				// +40
} UDF_tag_partition_header_descriptor  PACKED;			// +128

/* File Identifier Descriptor (ECMA-167 4/14.4) */
typedef struct {
	UDF_tag		DescriptorTag;				// +0   (id = 257)
	UDF_Uint16	FileVersionNumber;			// +16
	UDF_Uint8	FileCharacteristics;			// +18
		// bit 0: hidden
		// bit 1: directory
		// bit 2: deleted
		// bit 3: parent
		// bit 4: metadata
	UDF_Uint8	LengthOfFileIdentifier;			// +19
	UDF_long_ad	ICB;					// +20
	UDF_Uint16	LengthOfImplementationUse;		// +36
// END OF FIXED RECORDS						   +38
//	UDF_Uint8	ImplementationUse[LengthOfImplementationUse]; +38
//	UDF_Uint8	FileIdentifier[LengthOfFileIdentifier];	   +38 + LengthOfImplementationUse
} UDF_tag_file_identifier_descriptor  PACKED;			// ALIGN_BY_DWORD(38 + LengthOfImplementationUse + LengthOfFileIdentifier)

/* Allocation Extent Descriptor (ECMA-167 4/14.5) */
typedef struct {
	UDF_tag		DescriptorTag;				// +0   (id = 258)
	UDF_Uint32	PreviousAllocationExtentLocation;	// +16
	UDF_Uint32	LengthOfAllocationDescriptors;		// +20
	// allocation descriptors				   +24
} UDF_tag_allocation_extent_descriptor  PACKED;			// +24 + LengthOfAllocationDescriptors;

/* ICB tag */
typedef struct {
	UDF_Uint32	PriorRecordedNumberOfDirectEntries;	// +0
	UDF_Uint16	StrategyType;				// +4
	UDF_Uint16	StrategyParameter;			// +6
	UDF_Uint16	MaximumNumberOfEntries;			// +8
	UDF_Uint8	reserved;				// +10
	UDF_Uint8	FileType;				// +11
		// 0 = unspecified
		// 1 = unallocated space entry (ECMA-167 4/14.11)
		// 2 = partition integrity entry (ECMA-167 4/14.13)
		// 3 = indirect entry (ECMA-167 4/14.7)
		// 4 = directory (ECMA-167 4/8.6)
		// 5 = file
		// 6 = block device (ISO/IEC 9945-1)
		// 7 = character device (ISO/IEC 9945-1)
		// 8 = for recording extended attributes as described in (ECMA-167 4/9.1)
		// 9 = FIFO file (ISO/IEC 9945-1)
		// 10 = socket (ISO/IEC 9945-1)
		// 11 = terminal entry (ECMA-167 4/14.8)
		// 12 = symbolic link (ECMA-167 4/8.7)
		// 13 = stream directory (ECMA-167 4/9.2)
	UDF_lb_addr	ParentICBLocation;			// +12
	UDF_Uint16	Flags;					// +18
		// bits 0-2:
		//     0 = short allocation descriptors are used (ECMA-167 4/14.14.1)
		//     1 = long allocation descriptors are used (ECMA-167 4/14.14.2)
		//     2 = extended allocation descriptors are used (ECMA-167 4/14.14.3)
		//     3 = file contents are IN the area normally occupied by the Allocation Extent Descriptor (ick!)
		// bit 3:
		//     directory contents are sorted
		// bit 4: non-relocatable
		// bit 5: archive
		// bit 6: S_ISUID (ISO/IEC 9945-1)
		// bit 7: S_ISGID (ISO/IEC 9945-1)
		// bit 8: C_ISVTX (ISO/IEC 9945-1)
		// bit 9: contigious (file is stored sequentially, no fragmentation)
		// bit 10: system
		// bit 11: transformed
		// bit 12: multi-versions
		// bit 13: stream
} UDF_icbtag  PACKED;						// +20

/* Indirect entry (ECMA-167 4/14.7) */
typedef struct {
	UDF_tag		DescriptorTag;				// +0   (id = 259)
	UDF_icbtag	ICBTag;					// +16
	UDF_long_ad	IndirectICB;				// +36
} UDF_tag_indirect_entry_descriptor  PACKED;			// +52

/* Terminal Entry (ECMA-167 4/14.8) */
typedef struct {
	UDF_tag		DescriptorTag;				// +0   (id = 260)
	UDF_icbtag	ICBTag;					// +16
} UDF_tag_terminal_entry_descriptor  PACKED;			// +36

/* File Entry (ECMA-167 4/14.9) */
typedef struct {
	UDF_tag		DescriptorTag;				// +0   (id = 261)
	UDF_icbtag	ICBTag;					// +16
	UDF_Uint32	Uid;					// +36
	UDF_Uint32	Gid;					// +40
	UDF_Uint32	Permissions;				// +44  user/group/owner rwxrwxrwx
	UDF_Uint16	FileLinkCount;				// +48
	UDF_Uint8	RecordFormat;				// +50
		// 0 = unspecified
		// 1 = sequence of padded fixed-length records (ECMA-167 5/9.2.1)
		// 2 = ... fixed-length records (5/9.2.2)
		// 3 = ... variable length 8 records (5/9.2.3.1)
		// 4 = ... variable length 16 records (5/9.2.3.2)
		// 5 = ... variable length 16 MSB records (5/9.2.3.3)
		// 6 = ... variable length 32 records (5/9.2.3.4)
		// 7 = ... stream print records (5/9.2.4)
		// 8 = ... stream LF records (5/9.2.5)
		// 9 = ... stream CF records (5/9.2.6)
		// 10= ... stream CRLF records (5/9.2.7)
		// 11= ... stream LFCR records (5/9.2.8)
	UDF_Uint8	RecordDisplayAttributes;		// +51
		// 0 = unspecified
		// 1 = record is displayed on character imaging device ref 5/9.3.1
		// 2 = ... ref 5/9.3.2
		// 3 = ... ref 5/9.3.3
	UDF_Uint32	RecordLength;				// +52
	UDF_Uint64	InformationLength;			// +56
	UDF_Uint64	LogicalBlocksRecorded;			// +64
	UDF_timestamp	AccessDateAndTime;			// +72
	UDF_timestamp	ModificationDateAndTime;		// +84
	UDF_timestamp	AttributeDateAndTime;			// +96
	UDF_Uint32	Checkpoint;				// +108
	UDF_long_ad	ExtendedAttributeICB;			// +112
	UDF_regid	ImplementationIdentifier;		// +128
	UDF_Uint64	UniqueId;				// +160
	UDF_Uint32	LengthOfExtendedAttributes;		// +168
	UDF_Uint32	LengthOfAllocationDescriptors;		// +172
// END OF FIXED RECORDS						   +176
// 	UDF_Uint8	ExtendedAttributes[LengthOfExtendedAttributes]; +176
//	UDF_Uint8	AllocationDescriptors[LengthOfAllocationDescriptors]; +176 + LengthOfExtendedAttributes
// THIS IS NEEDED TO AVOID PROBLEMS WHEN WRITING ALLOCATION EXTENTS
	UDF_Uint8	allocations[2048-176];			// +176
} UDF_tag_file_entry_descriptor  PACKED;			// +176 + LengthOfExtendedAttributes + LengthOfAllocationDescriptors

/* Extended Attributes Header Descriptor (ECMA-167 4/4.10) */
typedef struct {
	UDF_tag		DescriptorTag;				// +0    (id = 262)
	UDF_Uint32	ImplementationAttributesLocation;	// +16
	UDF_Uint32	ApplicationAttributesLocation;		// +20
} UDF_tag_extended_attributes_header_descriptor  PACKED;

/* Extended Attributes generic format (ECMA-167 4/4.10.2) */
typedef struct {
	UDF_Uint32	AttributeType;				// +0
	UDF_Uint8	AttributeSubtype;			// +4
	UDF_Uint8	reserved[3];				// +5
	UDF_Uint32	AttributeLength;			// +8
	UDF_Uint8	AttributeData[1];			// +12 length = AttributeLength
} UDF_extended_attribute_generic  PACKED;			// +12 + AttributeLength

/* Extended Attribute: Character Set Information (ECMA-167 4/14.10.3) */
// TODO

/* Extended Attribute: Alternate Permissions (ECMA-167 4/14.10.4) */
// TODO

/* Extended Attribute: File Times (ECMA-167 4/14.10.5) */
// TODO

/* Extended Attribute: Information Times (ECMA-167 4/14.10.6) */
// TODO

/* Extended Attribute: Device Specification (ECMA-167 4/14.10.7) */
// TODO

/* Extended Attribute: Implementation Use (ECMA-167 4/14.10.8) */
// TODO

/* Extended Atrribute: Application Use (ECMA-167 4/14.10.9) */
// TODO

/* Unallocated Space Entry Descriptor (ECMA-167 4/14.11) */
typedef struct {
	UDF_tag		DescriptorTag;				// +0    (id = 263)
	UDF_icbtag	ICBTag;					// +16
	UDF_Uint32	LengthOfAllocationDescriptors;		// +36
// Allocation Descriptors
} UDF_unallocated_space_entry_descriptor  PACKED;

/* Space Bitmap Descriptor (ECMA-167 4/14.12) */
typedef struct {
	UDF_tag		DescriptorTag;				// +0    (id = 264)
	UDF_Uint32	NumberOfBits;				// +16
	UDF_Uint32	NumberOfBytes;				// +20
// space bitmap							   +24
//     logical bit n:
//         byte(n/8)  bit(n%8)
} UDF_space_bitmap_descriptor  PACKED;

/* Partition Integrity Entry (ECMA-167 4/14.13) */
typedef struct {
	UDF_tag		DescriptorTag;				// +0    (id = 265)
	UDF_icbtag	ICBTag;					// +16
	UDF_timestamp	RecordingDateAndTime;			// +36
	UDF_Uint8	IntegrityType;				// +48
	UDF_Uint8	reserved[175];				// +49
	UDF_regid	ImplementationIdentifier;		// +224
	UDF_Uint8	ImplementationUse[256];			// +256
} UDF_partition_integrity_entry_descriptor  PACKED;		// +512

/* Logical Volume Header Descriptor (ECMA-167 4/14.15) */
// TODO

/* Extended File Entry Descriptor (ECMA-167 4/14.17) */
typedef struct {
	UDF_tag		DescriptorTag;				// +0    (id = 266)
	UDF_icbtag	ICBTag;					// +16
	UDF_Uint32	Uid;					// +36
	UDF_Uint32	Gid;					// +40
	UDF_Uint32	Permissions;				// +44
	UDF_Uint16	FileLinkCount;				// +48
	UDF_Uint8	RecordFormat;				// +50
	UDF_Uint8	RecordDisplayAttributes;		// +51
	UDF_Uint32	RecordLength;				// +52
	UDF_Uint64	InformationLength;			// +56
	UDF_Uint64	ObjectSize;				// +64
	UDF_Uint64	LogicalBlocksRecorded;			// +72
	UDF_timestamp	AccessDateAndTime;			// +80
	UDF_timestamp	ModificationDateAndTime;		// +92
	UDF_timestamp	CreationDateAndTime;			// +104
	UDF_timestamp	AttributeDateAndTime;			// +116
	UDF_Uint32	Checkpoint;				// +128
	UDF_Uint32	Reserved;				// +132
	UDF_long_ad	ExtendedAttributeICB;			// +136
	UDF_long_ad	StreamDirectoryICB;			// +152
	UDF_regid	ImplementationIdentifier;		// +168
	UDF_Uint64	UniqueId;				// +200
	UDF_Uint32	LengthOfExtendedAttributes;		// +208
	UDF_Uint32	LengthOfAllocationDescriptors;		// +212
// END OF FIXED LENGTH RECORDS					// +216
// 	UDF_Uint8	ExtendedAttributes[LenOfExtAttributes];	// +216
//	UDF_Uint8	AllocationDescriptors[LenOfAllocDesc];	// +216 + LengthOfExtAttr
} UDF_tag_extended_file_entry_descriptor  PACKED;

/* ECMA-167 (3/8.4.2) Recording of a Volume Descriptor Sequence:
 *
 * [Volume Descriptor Sequence extent]{
 *   <volume descriptor>0+
 *     [Terminator]{
 *       <Volume Descriptor Pointer>
 *     | <Terminating Descriptor>
 *     | <unrecorded logical sector>
 *   } <trailing logical sector>0+
 * }
 *
 * Sequence defined by Anchor Tag extent
 */

/* UDF v1.02 extension to the Logical Volume Integrity Descriptor (implemenation use area) */
typedef struct {
	UDF_regid	ImplementationID;		// +0
	UDF_Uint32	NumberOfFiles;			// +32
	UDF_Uint32	NumberOfDirectories;		// +36
	UDF_Uint16	MinimumUDFReadRevision;		// +40
	UDF_Uint16	MinimumUDFWriteRevision;	// +42
	UDF_Uint16	MaximumUDFWriteRevision;	// +44
} UDF_tag_logical_volume_integrity_descriptor_implementation_use_UDFv102; // +46

#endif //_UDF_H
