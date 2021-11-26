/*
 * mkudfiso.cpp
 *
 * mkudfiso pure UDF ISO authoring tool for use in authoring pure UDF CD-ROM/DVD/HD-DVD/Bluray discs.
 * Copyright (C) 2007 Impact Studio Pro LLC.
 * Written by Jonathan Campbell.
 *
 * Licensed under the GNU General Public License for Personal Non-Commercial Use Only.
 * Commercial Use requires a commercial license from Impact Studio Pro.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <time.h>

#include "sha256.h"
#include "sha1.h"
#include "md5.h"

#include "bytes.h"
#include "udf.h"

#include <assert.h>
#include <string>
#include <iostream>
#include <utility>
#include <ostream>
#include <fstream>
#include <list>
#include <map>

//#define EMIT_RESERVE_VOLUME_DESCRIPTOR

#if defined(__i386__)
/* 32-bit x86 GLIBC needs to be reminded how to use lseek64(). x86-64 doesn't need this. */
signed long long lseek64(int fd,signed long long ofs,int whence);
#endif

using namespace std;

/* variables */
static UDF_timestamp	volume_recordtime;
static UDF_Uint64	iso_size_limit = 0;	/* we can error out and tell the shell script we can't fit it below this limit */
						/* this is preferable to mkisofs silently dropping files from the iso if they
						 * fail a certain criteria, like being >= 4GB, grrrr >:{ */
static string		content_root;		/* the directory who's contents we package into the ISO */
static string		iso_file;
static string		volume_label = "";
static string		volume_set_identifier = "";
static string		invoked_root;

static string		report_file;		/* a "report file" about the ISO created. use this to locate and recover the files
						   in case the UDF structure is invalid */
static string		hashtable_file;		/* a "hash file" with hashes of all files */
static string		gap_file;		/* a "gap file" contains a list of unused areas of the disc */

static int		auto_sparse_detect=0;	/* 1=detect holes (runs of zeros) in files and mark them as "not allocated not recorded" extents.
						   this makes them sparse files. the runs of zeros can then be reused for other purposes. */
static int		iso_overwrite=0;	/* 1=if ISO exists, overwrite it. else, return error */

UDF_Uint32 PartitionStart = 0;
UDF_Uint32 PartitionTagSector = 0;

UDF_Uint64 metric_atoi(char *s) {
	char *t = NULL;
	UDF_Uint64 v = strtoull(s,&t,0);
	if (t) {
		/* see if the number is postfixed by 'MB', 'GB', etc... */
		if (!strncmp(t,"KB",2))
			v <<= 10LL;
		else if (!strncmp(t,"MB",2))
			v <<= 20LL;
		else if (!strncmp(t,"GB",2))
			v <<= 30LL;
		else if (!strncmp(t,"TB",2))
			v <<= 40LL;
	}
	return v;
}

/* OSTA Checksum routine */
/*
 * * CRC 010041
 * */
static unsigned short osta_crc_table[256] = {
	0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
	0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
	0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
	0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
	0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
	0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
	0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
	0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
	0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
	0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
	0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12,
	0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
	0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41,
	0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
	0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
	0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
	0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F,
	0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
	0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E,
	0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
	0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
	0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
	0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C,
	0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
	0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB,
	0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
	0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
	0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
	0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9,
	0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
	0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8,
	0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0
};
unsigned short osta_cksum(unsigned char *s,int n)
{
	register unsigned short crc=0;
	while (n-- > 0) crc = osta_crc_table[(crc>>8 ^ *s++) & 0xff] ^ (crc<<8);
	return crc;
}

static void UDF_timestamp_set(UDF_timestamp &ut,time_t tt)
{
	struct tm *t = gmtime(&tt);
	ut.TypeAndTimeZone = (0 << 12) | 0;	/* UTC */
	ut.Year = t->tm_year + 1900;
	ut.Month = t->tm_mon + 1;
	ut.Day = t->tm_mday;
	ut.Hour = t->tm_hour;
	ut.Minute = t->tm_min;
	ut.Second = t->tm_sec;
	ut.Centiseconds = 0;
	ut.HundredsOfMicroseconds = 0;
	ut.Microseconds = 0;
}

UDF_Uint32 UnixToUDF(unsigned long p) {
	UDF_Uint32 r = (p & 7) | (((p >> 3) & 7) << 5) | (((p >> 6) & 7) << 10);
	return r;
}

class FileEntry {
	public:
		FileEntry() {
			id = parent = 0;
			gid = uid = -1;
			permissions = 0x14A5;
			characteristics = 0;
			file_size = 0;
			UDF_timestamp_set(file_atime,0);
			UDF_timestamp_set(file_ctime,0);
			UDF_timestamp_set(file_mtime,0);
			hash_length = 0;
		}
	public:
		UDF_Uint64	id,parent;		/* used to build parent/child relationship */
		UDF_Uint64	file_size;
		UDF_timestamp	file_atime;
		UDF_timestamp	file_ctime;
		UDF_timestamp	file_mtime;
		UDF_Uint32	permissions;
		UDF_Uint32	uid,gid;
		UDF_Uint8	characteristics;	/* UDF characteristics */
		string		name;
		string		abspath;
		string		path;
	public:
		sha256_context	sha256_ctx;
		UDF_Uint8	sha256[32];
		sha1_context	sha1_ctx;
		UDF_Uint8	sha1[20];
		md5_context	md5_ctx;
		UDF_Uint8	md5[16];
		UDF_Uint64	hash_length;
};

typedef struct SingleSectorGap {
	unsigned int	start,end;
} SingleSectorGap;

map<UDF_Uint64,FileEntry>	file_list;
UDF_Uint64			file_list_total = 0;
map<UDF_Uint64,UDF_Uint64>	parent_dir_to_first_file;
map<UDF_Uint64,SingleSectorGap>	single_sector_gaps;

class OutputExtent {
	public:
		OutputExtent() {
			file = NULL;
			content = NULL;
			content_length = 0;
			start = end = 0;
		}
		~OutputExtent() {
			clear();
		}
		void clear() {
			if (content) delete content;
			content_length = 0;
			content = NULL;
			file = NULL;
		}
		void setContent(void *buf,int len) {
			clear();
			if (len <= 0) return;
			content_length = len;
			content = new UDF_Uint8[len];
			if (!content) return;
			memcpy(content,buf,len);
		}
		void setFile(FileEntry *f) {
			clear();
			file = f;
		}
		void setRange(UDF_Uint64 _start,UDF_Uint64 _size) {
			start = _start;
			end = start + _size;
		}
	public:
		FileEntry*	file;			// non-NULL if this refers to a file
		UDF_Uint8*	content;		// actual contents
		int		content_length;
		UDF_Uint64	start,end;		// starting/ending sectors (start <= x < end)
};

map<UDF_Uint64,OutputExtent>	output_extents;
static UDF_Uint64		output_extents_solid = 16;

OutputExtent *NewOutputExtent(UDF_Uint64 start=0,UDF_Uint64 size=1) {
	if (start == 0) {
		map<UDF_Uint64,OutputExtent>::iterator i = output_extents.lower_bound(output_extents_solid);
		if (i == output_extents.end()) start = 16;
		else {
			start = 0;
			do {
				UDF_Uint64 first = i->second.start;
				UDF_Uint64 last = i->second.end;
				i++;

				UDF_Uint64 next = 0;
				if (i != output_extents.end()) next = i->second.start;

				if (next && last >= next) {
					output_extents_solid = next;
				}
				else if (next == 0) {
					start = last;
					break;
				}
				else if ((last+size) <= next) {
					start = last;
					break;
				}
			} while (i != output_extents.end());
		}

		assert(start >= 16);
	}

	OutputExtent *e = &output_extents[start];
	e->setRange(start,size);
	return e;
}

string humanize(UDF_Uint64 s) {
	char *suffix = "b";
	int fraction = 0; // 0-999 only (3 digits)
	char tmp[64];

	if (s >= 1024) {
		suffix = "KB";
		fraction = ((((unsigned int)s)&0x3FF) * 1000) >> 10;
		s >>= 10LL;
	}
	if (s >= 1024) {
		suffix = "MB";
		fraction = ((((unsigned int)s)&0x3FF) * 1000) >> 10;
		s >>= 10LL;
	}
	if (s >= 1024) {
		suffix = "GB";
		fraction = ((((unsigned int)s)&0x3FF) * 1000) >> 10;
		s >>= 10LL;
	}
	if (s >= 1024) {
		suffix = "TB";
		fraction = ((((unsigned int)s)&0x3FF) * 1000) >> 10;
		s >>= 10LL;
	}

	if (fraction != 0)
		sprintf(tmp,"%Lu.%03u",s,fraction);
	else
		sprintf(tmp,"%Lu",s);

	string f = string(tmp) + string(suffix);
	return f;
}

/* This is expected to allocate IDs in sequential order */
UDF_Uint64 file_list_alloc() {
	map<UDF_Uint64,FileEntry>::reverse_iterator i = file_list.rbegin();
	if (file_list.rend() == i) return 1;
	return i->first+1;
}

/* alternative chdir() so that we can exceed MAX_PATH if necessary */
int extra_large_chdir(const char *path) {
	if (*path != '/') {
		fprintf(stderr,"extra_large_chdir() %s is not an absolute path\n",path);
		return -1;
	}

	while (*path == '/')
		path++;

	if (chdir("/") < 0) {
		fprintf(stderr,"extra_large_chdir() Cannot even enter '/' %s\n",strerror(errno));
		return -1;
	}

	const char *x = path;
	while (*x) {
		/* compute from here to the next '/' or end of string */
		const char *e = strchr(x,'/');
		if (!e) e = x + strlen(x);

		/* make the lone element into a C++ string */
		string elem = string(x,(int)(e-x));
		x = e; while (*x == '/') x++;
		if (chdir(elem.c_str()) < 0) {
			fprintf(stderr,"extra_large_chdir() Cannot enter part %s of path %s\n",elem.c_str(),path);
			return -1;
		}
	}

	return 0;
}

static int scan_contents(const char *basepath,UDF_Uint64 base_id=0) {
	if (extra_large_chdir(basepath) < 0) {
		fprintf(stderr,"Cannot enter %s\n",basepath);
		return 0;
	}

	DIR *dir;
	if ((dir = opendir(".")) == NULL) {
		fprintf(stderr,"Cannot read directory %s\n",basepath);
		return 0;
	}

	int idcount=0;
	struct dirent *de;
	list<UDF_Uint64> new_ids;
	while ((de = readdir(dir)) != NULL) {
		if (!strcmp(de->d_name,".") || !strcmp(de->d_name,".."))
			continue;

		string abs_path = string(basepath) + string("/") + string(de->d_name);

		struct stat64 st;
		if (lstat64(de->d_name,&st) < 0) {
			fprintf(stderr,"Cannot stat %s/%s, ignoring\n",basepath,de->d_name);
			continue;
		}
		if (S_ISLNK(st.st_mode)) {
			fprintf(stderr,"%s/%s is a symbolic link, which is not supported yet\n",basepath,de->d_name);
			continue;
		}
		if (!S_ISREG(st.st_mode) && !S_ISDIR(st.st_mode)) {
			fprintf(stderr,"%s/%s is not a file, ignoring\n",basepath,de->d_name);
			continue;
		}

		/* it's pretty silly to associate size with directories */
		if (S_ISDIR(st.st_mode))
			st.st_size = 0;

		UDF_Uint64 id = file_list_alloc();
		FileEntry *fl = &file_list[id];
		fl->id = id;
		fl->parent = base_id;
		fl->abspath = abs_path;
		fl->path = basepath;
		fl->name = de->d_name;
//		fl->permissions = UnixToUDF(st.st_mode);
		fl->uid = st.st_uid;
		fl->gid = st.st_gid;
		fl->file_size = st.st_size;
		fl->characteristics = (S_ISDIR(st.st_mode) ? 2 : 0);
		UDF_timestamp_set(fl->file_atime,st.st_atime);
		UDF_timestamp_set(fl->file_ctime,st.st_ctime);
		UDF_timestamp_set(fl->file_mtime,st.st_mtime);

		if (S_ISDIR(st.st_mode))
			new_ids.push_back(id);
		else
			file_list_total += fl->file_size;

		if (idcount == 0) parent_dir_to_first_file[base_id] = id;
		idcount++;
	}
	closedir(dir);

	{
		list<UDF_Uint64>::iterator i;
		for (i=new_ids.begin();i != new_ids.end();i++) {
			UDF_Uint64 parent_id = *i;
			FileEntry *parent = &file_list[parent_id];
			scan_contents(parent->abspath.c_str(),parent_id);
		}
	}

	return 1;
}

static int parse_args(int argc,char **argv) {
	int i,nonsw=0;
	char forget=0;

	for (i=1;i < argc;) {
		char *a = argv[i++];

		if (*a == '-' && !forget) {
			char *sw = a+1;
			if (*sw == '-') sw++;

			/* -- means treat further arguments starting with '-' as file names, etc. */
			if (*sw == 0) {
				forget = 1;
			}
			else if (!strcmp(sw,"limit")) {
				char *e = argv[i++];
				if (!e) continue;
				iso_size_limit = metric_atoi(e);
			}
			else if (!strcmp(sw,"o")) {
				char *e = argv[i++];
				if (!e) continue;
				if (*e == '/')	iso_file = e;
				else		iso_file = invoked_root + string("/") + string(e);
			}
			else if (!strcmp(sw,"report")) {
				char *e = argv[i++];
				if (!e) continue;
				if (*e == '/')	report_file = e;
				else		report_file = invoked_root + string("/") + string(e);
			}
			else if (!strcmp(sw,"hashes")) {
				char *e = argv[i++];
				if (!e) continue;
				if (*e == '/')	hashtable_file = e;
				else		hashtable_file = invoked_root + string("/") + string(e);
			}
			else if (!strcmp(sw,"gap")) {
				char *e = argv[i++];
				if (!e) continue;
				if (*e == '/')	gap_file = e;
				else		gap_file = invoked_root + string("/") + string(e);
			}
			else if (!strcmp(sw,"sparse")) {
				auto_sparse_detect = 1;
			}
			else if (!strcmp(sw,"force-iso")) {
				iso_overwrite = 1;
			}
			else if (!strcmp(sw,"v") || !strcmp(sw,"volume")) {
				char *e = argv[i++];
				if (!e) continue;
				volume_label = e;
			}
			else if (!strcmp(sw,"help")) {
				fprintf(stderr,"mkudfiso (C) 2007, 2008 Jonathan Campbell, Impact Studio Pro\n");
				fprintf(stderr,"Compile files and directories into a pure UDF filesystem\n");
				fprintf(stderr,"For personal noncommercial use ONLY\n");
				fprintf(stderr,"\n");
				fprintf(stderr,"mkudfiso [options] <directory to compile into ISO>\n");
				fprintf(stderr,"  -limit <size>    Error out if resulting ISO will exceed this limit\n");
				fprintf(stderr,"       size can be a number in bytes followed by KB,MB,GB,TB\n");
				fprintf(stderr,"            CD-ROM     640MB\n");
				fprintf(stderr,"            DVD-R      4482MB\n");
				fprintf(stderr,"            DVD-R+DL   8105MB\n");
				fprintf(stderr,"            BD-ROM     2336GB\n");
				fprintf(stderr,"  -report <file>   Generate a report about the ISO. <file> will be a text file\n");
				fprintf(stderr,"                   If space is available, the report is added to the ISO file\n");
				fprintf(stderr,"  -force-iso       Overwrite ISO file if it already exists\n");
				fprintf(stderr,"  -sparse          Detect long runs of zero sectors and make the file sparse\n");
				fprintf(stderr,"  -v <label>       Set volume label\n");
				return 0;
			}
			else {
				fprintf(stderr,"Unknown switch %s\n",sw);
				return 0;
			}
		}
		else {
			switch (nonsw++) {
				case 0:
					content_root = a;
					break;
				default:
					fprintf(stderr,"Ignored: %s\n",a);
			}
		}
	}

	if (content_root.length() < 1) {
		fprintf(stderr,"You must specify a directory who's contents are to be made into a UDF filesystem\n");
		return 0;
	}

	return 1;
}

static void UDF_subdirectory(UDF_short_ad* DirExtent,OutputExtent* parent,UDF_Uint64 dir_id,OutputExtent* self) {
	UDF_tag_file_entry_descriptor *DirFileEntryTag =
		(UDF_tag_file_entry_descriptor*)(self->content);
	UDF_short_ad *DirFileEntryTagExtent =
		(UDF_short_ad*)(self->content + 176);

	/* now generate the root directory */
	OutputExtent *DirDirectory = NULL; {
		map<UDF_Uint64,FileEntry>::iterator i;
		FileEntry *oex = NULL;
		int alloc_sz = 40;	/* room for . and .. */

/* how much memory/space do we need to create the directory? */
		for (	i  = file_list.find(parent_dir_to_first_file[dir_id]);
			i != file_list.end() && i->second.parent == dir_id;i++) {
			oex = &i->second;
			int sz =
				16 + 2 + 1 + 1 + 16 + 2 +
				0 + /* Length of Implementation Use */
				/* null-length Implementation Use */
				oex->name.length() + 1;	/* File Identifier (as a d-string) */
			sz = (sz + 3) & (~3);		/* padding (up to next DWORD) */
			alloc_sz += sz;
		}
		if (alloc_sz == 0) {
			cerr << "ERROR: Program bug: Apparently there is no directory" << endl;
			exit(1);
		}

/* create the directory */
		DirDirectory = NewOutputExtent(0,(alloc_sz+2047) >> 11);
		unsigned char *dir_raw = new unsigned char[alloc_sz];
		unsigned char *dir_cur = dir_raw;
		memset(dir_raw,0,alloc_sz);

		/* . and .. */
		{
			UDF_tag_file_identifier_descriptor *fent =
				(UDF_tag_file_identifier_descriptor*)dir_cur;
			SET_UDF_tag(fent->DescriptorTag,UDFtag_FileIdentifierDescriptor,
				DirDirectory->start - PartitionStart);
			UPDATE_UDF_tag(fent->DescriptorTag);
			fent->FileVersionNumber = 1;
			fent->FileCharacteristics = 0x0A;	/* parent node */
			fent->LengthOfFileIdentifier = 0;
			fent->ICB.ExtentLength = 2048;
			fent->ICB.ExtentLocation.LogicalBlockNumber = parent->start - PartitionStart;	/* parent */
			fent->ICB.ExtentLocation.PartitionReferenceNumber = 0;
			SET_UDF_tag_checksum(fent->DescriptorTag,2);
			dir_cur += 40;
#if 0
			/* argh apparently file link counts matter to every UDF reader out there,
			 * so update the paren't file link count field */
			{
				UDF_tag_file_entry_descriptor *fed =
					(UDF_tag_file_entry_descriptor*)(parent->content);
				UDF_Uint32 v = LGETDWORD(&fed->FileLinkCount);
				LSETDWORD(&fed->FileLinkCount,(v+1));
				SET_UDF_tag_checksum(fed->DescriptorTag,parent->content_length-16);
			}
#endif
		}

		list< pair<UDF_Uint64,FileEntry*> > dir_ents;
		list< pair<UDF_Uint64,FileEntry*> > file_ents;
		for (	i  = file_list.find(parent_dir_to_first_file[dir_id]);
			i != file_list.end() && i->second.parent == dir_id;i++) {
			oex = &i->second;
			int sz =
				16 + 2 + 1 + 1 + 16 + 2 +
				0 + /* Length of Implementation Use */
				/* null-length Implementation Use */
				oex->name.length() + 1;	/* File Identifier (as a d-string) */
			sz = (sz + 3) & (~3);		/* padding (up to next DWORD) */

			if ((dir_cur + sz) > (dir_raw + alloc_sz)) {
				cerr << "ERROR: Program bug: Miscalculation of an entry in the root dir" << endl;
				exit(1);
			}

			/* create the file entries to make them happen */
			int sectorsneeded = 1;
			/* TODO: For files larger than 231GB, multiple sectors are needed for the allocation extent array */
			OutputExtent *FileEntry2 = NewOutputExtent(0,sectorsneeded);
			unsigned char FileEntry2TagRaw[2048];
			memset(FileEntry2TagRaw,0,sizeof(FileEntry2TagRaw));
			UDF_tag_file_entry_descriptor &FileEntry2Tag = *((UDF_tag_file_entry_descriptor*)FileEntry2TagRaw);
			memset(&FileEntry2Tag,0,sizeof(FileEntry2Tag));
			SET_UDF_tag(FileEntry2Tag.DescriptorTag,UDFtag_FileEntry,
				FileEntry2->start - PartitionStart);
			UPDATE_UDF_tag(FileEntry2Tag.DescriptorTag);
			FileEntry2Tag.ICBTag.PriorRecordedNumberOfDirectEntries = 0;
			FileEntry2Tag.ICBTag.StrategyType = 4;
			LSETWORD(&FileEntry2Tag.ICBTag.StrategyParameter,0);
			FileEntry2Tag.ICBTag.MaximumNumberOfEntries = 1;
			FileEntry2Tag.ICBTag.FileType = (oex->characteristics & 2) ? 4 : 5; /* file or folder? */
			FileEntry2Tag.ICBTag.ParentICBLocation.LogicalBlockNumber = parent->start - PartitionStart;
			FileEntry2Tag.ICBTag.ParentICBLocation.PartitionReferenceNumber = 0;
			FileEntry2Tag.ICBTag.Flags = 0x230;	/* non-relocatable, short_ad */
			FileEntry2Tag.Uid = -1;
			FileEntry2Tag.Gid = -1;
			FileEntry2Tag.Permissions = oex->permissions;
			FileEntry2Tag.FileLinkCount = 1;
			FileEntry2Tag.RecordFormat = 0;
			FileEntry2Tag.RecordDisplayAttributes = 0;
			FileEntry2Tag.RecordLength = 0;
			FileEntry2Tag.InformationLength = oex->file_size;
			FileEntry2Tag.LogicalBlocksRecorded = (oex->file_size + 2047LL) >> 11LL;
			FileEntry2Tag.AccessDateAndTime = oex->file_atime;
			FileEntry2Tag.AttributeDateAndTime = oex->file_ctime;
			FileEntry2Tag.ModificationDateAndTime = oex->file_mtime;
			FileEntry2Tag.Checkpoint = 1;
			FileEntry2Tag.UniqueId = oex->id;
			SET_UDF_regid(FileEntry2Tag.ImplementationIdentifier,0,"*mkudfiso","");
			/* set aside directories for later */
			if (oex->characteristics & 2) {
				/* add to list */
				pair<UDF_Uint64,FileEntry*> po(FileEntry2->start,&file_list[i->first]);
				dir_ents.push_back(po);
			}
			/* if the file is small enough we can stick the contents directly INTO the descriptor
			 * where the allocation extents normally go. See ECMA-167 4/14.6 "ICB tag" */
			else if (FileEntry2Tag.InformationLength < (2048-176)) {
				FileEntry2Tag.ICBTag.Flags = 0x233;	/* non-relocateable, the allocation extent area IS the file content */
				FileEntry2Tag.LengthOfAllocationDescriptors = FileEntry2Tag.InformationLength;
				FileEntry2Tag.LogicalBlocksRecorded = 0;

				if (extra_large_chdir(oex->path.c_str()) < 0) {
					cerr << "Cannot enter " << oex->path << endl;
				}
				else {
					int fd = open(oex->name.c_str(),O_RDONLY);
					if (fd < 0) {
						cerr << "Cannot open " << oex->abspath << endl;
					}
					else {
						int r = read(fd,((unsigned char*)(&FileEntry2Tag))+176,FileEntry2Tag.InformationLength);
						if (r < FileEntry2Tag.InformationLength)
							cerr << "WARNING: Read less data than expected for " << oex->abspath << endl;
						close(fd);
					}
				}
			}
			else {
				/* add to list */
				pair<UDF_Uint64,FileEntry*> po(FileEntry2->start,&file_list[i->first]);
				file_ents.push_back(po);
			}
			SET_UDF_tag_checksum(FileEntry2Tag.DescriptorTag,2);
			FileEntry2->setContent(&FileEntry2Tag,2048);

			/* create the directory entry */
			UDF_tag_file_identifier_descriptor *fent =
				(UDF_tag_file_identifier_descriptor*)dir_cur;
			SET_UDF_tag(fent->DescriptorTag,UDFtag_FileIdentifierDescriptor,
				DirDirectory->start - PartitionStart);
			UPDATE_UDF_tag(fent->DescriptorTag);
			fent->FileVersionNumber = 1;
			fent->FileCharacteristics = oex->characteristics;
			fent->LengthOfFileIdentifier = oex->name.length()+1;
			fent->ICB.ExtentLength = 2048;
			fent->ICB.ExtentLocation.LogicalBlockNumber = FileEntry2->start - PartitionStart;
			fent->ICB.ExtentLocation.PartitionReferenceNumber = 0;
			UDF_dstring_strncpyne((dir_cur+38),(oex->name.length()+1),oex->name.c_str());
			SET_UDF_tag_checksum(fent->DescriptorTag,2);

			/* advance */
			dir_cur += sz;
		}
		DirDirectory->setContent(dir_raw,alloc_sz);
		delete dir_raw;

		DirFileEntryTag->InformationLength = alloc_sz;
		DirFileEntryTag->LogicalBlocksRecorded = (alloc_sz + 2047LL) >> 11LL;
		DirFileEntryTagExtent->ExtentLength = alloc_sz;
		DirFileEntryTagExtent->ExtentPosition = DirDirectory->start - PartitionStart;
		DirFileEntryTag->LengthOfAllocationDescriptors = 8;
		SET_UDF_tag_checksum(DirFileEntryTag->DescriptorTag,2);

		pair<UDF_Uint64,FileEntry*> prpair;
		list< pair<UDF_Uint64,FileEntry*> >::iterator pri;

		/* put folders in */
		for (pri=dir_ents.begin();pri != dir_ents.end();pri++) {
			UDF_Uint64 sector = pri->first;
			FileEntry* file = pri->second;
			OutputExtent *sx = &output_extents[sector];
			UDF_subdirectory(DirFileEntryTagExtent,self,file->id,sx);
		}
		dir_ents.clear();

		/* put files in */
		for (pri=file_ents.begin();pri != file_ents.end();pri++) {
			UDF_Uint64 sector = pri->first;
			FileEntry* file = pri->second;
			OutputExtent *sx = &output_extents[sector];
			if (!sx->content) {
				cerr << "Unexpected: Sector " << sector << " does not have contents" << endl;
				continue;
			}

			UDF_tag_file_entry_descriptor *fed =
				(UDF_tag_file_entry_descriptor*)(sx->content);

			int allocs_needed = 1;
			UDF_Uint64 ssz = fed->InformationLength;
			UDF_short_ad *sad = (UDF_short_ad*)(sx->content + 176);

			/* how many allocation extents */
			while (ssz >= 1048576000) {
				ssz -= 1048576000;
				allocs_needed++;
			}

			fed->LengthOfAllocationDescriptors =
				allocs_needed * 8;

			/* make the output extent for the file */
			OutputExtent *fex = NewOutputExtent(0,fed->LogicalBlocksRecorded);

			/* build extents */
			{
				UDF_Uint32 s = fex->start - PartitionStart;
				ssz = fed->InformationLength;
				while (ssz >= 1048576000) {
					LSETDWORD(&sad->ExtentLength,1048576000);
					LSETDWORD(&sad->ExtentPosition,s);
					sad = (UDF_short_ad*)(((unsigned char*)sad) + 8);
					s += 1048576000 >> 11;
					ssz -= 1048576000;
				}
				if (ssz > 0) {
					LSETDWORD(&sad->ExtentLength,((UDF_Uint32)ssz));
					LSETDWORD(&sad->ExtentPosition,s);
					sad = (UDF_short_ad*)(((unsigned char*)sad) + 8);
					s += (ssz + 2047) >> 11;
					ssz = 0;
				}
				if ((s+PartitionStart) != fex->end) {
					cerr << "BUG: Extent computation ended at " << s << " expected " << fex->end << endl;
				}
				if (allocs_needed != ((((unsigned char*)sad) - (sx->content + 176))>>3)) {
					cerr << "BUG: Miscalculated the number of extents" << endl;
				}
			}

			fex->setFile(file);
			SET_UDF_tag_checksum(fed->DescriptorTag,2);
		}
		file_ents.clear();
	}
}

int main(int argc,char **argv) {
	{
		char path[4096];
#if 0
		/* most compatible: getcwd */
		// TODO
#else
		/* Linux-specific, use procfs to read our cwd */
		{
			char tmp[128];
			sprintf(tmp,"/proc/%d/cwd",getpid());
			int fd = open(tmp,O_RDONLY);
			if (fd < 0) {
				cerr << "Cannot get cwd" << endl;
				return 1;
			}
			int rd = readlink(tmp,path,4095);
			if (rd < 0) rd = 0;
			path[rd] = 0;
			close(fd);
		}
		invoked_root = path;
#endif
	}

	if (!parse_args(argc,argv))
		return 1;

	/* CRC self-check */
	{
		unsigned char test[] = { 0x70, 0x6A, 0x77 };
		if (osta_cksum(test,sizeof(test)) != 0x3299) {
			cerr << "CRC self-test failure" << endl;
			return 1;
		}
	}

	/* important checks that GCC may miss */
	assert(sizeof(UDF_lb_addr) == 6);

	srand(time(NULL) + (getpid() * 7729));
	UDF_timestamp_set(volume_recordtime,time(NULL));

	if (isatty(1))
		printf("Scanning directory...\n");

	if (scan_contents(content_root.c_str()) < 0) return 1;

	if (isatty(1))
		cout << "* Raw total: " << humanize(file_list_total) << endl;

	if (iso_size_limit && file_list_total > iso_size_limit) {
		cerr << "ERROR: The sum of all your files exceed the ISO limit you specified" << endl;
		return 1;
	}

	OutputExtent *BEA01 = NewOutputExtent(16); {
		UDF_volumedescriptor_BEA i; assert(sizeof(i) == 2048);
		memset(&i,0,sizeof(i)); memcpy(i.StandardIdentifier,"BEA01",5);
		i.VolumeDescriptorVersion = 1; BEA01->setContent(&i,32);
	}
	OutputExtent *NSR02 = NewOutputExtent(17); {
		UDF_volumedescriptor_NSR i; assert(sizeof(i) == 2048);
		memset(&i,0,sizeof(i)); memcpy(i.StandardIdentifier,"NSR02",5);
		i.VolumeDescriptorVersion = 1; NSR02->setContent(&i,32);
	}
	OutputExtent *TEA01 = NewOutputExtent(18); {
		UDF_volumedescriptor_BEA i; assert(sizeof(i) == 2048);
		memset(&i,0,sizeof(i)); memcpy(i.StandardIdentifier,"TEA01",5);
		i.VolumeDescriptorVersion = 1; TEA01->setContent(&i,32);
	}
	/* mkisofs does this, why not us too? :) */
	OutputExtent *BraggingRights = NewOutputExtent(); {
		unsigned char data[2048];
		memset(data,0,2048);
		time_t t = time(NULL);
		snprintf((char*)data,2047,
			"mkudfiso v0.2 UDF authoring tool (C) 2007, 2008 Impact Studio Pro. \"%s\" -> \"%s\" on %s",
			content_root.c_str(),
			iso_file.length() > 0 ? iso_file.c_str() : "(stdout)",
			ctime(&t));

		/* no really, how much space does this bragging occupy? */
		int sz = strlen((char*)data)+1;
		if (sz > 2048) sz = 2048;

		BraggingRights->setContent(data,sz);
	}

	/* most UDF authoring tools align this to a 16 sector boundary.
	 * this is OK if you're concerned with readability but I don't think it's strictly necessary. */
#if 0
	{
		OutputExtent *Padding = NewOutputExtent(); {
			Padding->end = 32;
		}
	}
#endif

	UDF_Uint32 rootfileset_n;
	UDF_Uint32 rootfileent_n;
	UDF_Uint32 rootterm_n;
	UDF_Uint32 rootdir_n;

	/* Volume Descriptor Sequence Extent */
	OutputExtent *VolumeDescriptorSequenceExtent = NewOutputExtent(0,6); {
		unsigned char data[2048*6];
		memset(data,0,sizeof(data));

		{
			// <here> + how many sectors we will occupy + 2 sectors for Logical Volume Integrity descriptor
#ifdef EMIT_RESERVE_VOLUME_DESCRIPTOR
			const UDF_Uint32 begin = VolumeDescriptorSequenceExtent->start + 12 + 2;
#else
			const UDF_Uint32 begin = VolumeDescriptorSequenceExtent->start + 6 + 2;
#endif
			rootfileset_n = begin+0;
			rootterm_n = begin+1;
			rootfileent_n = begin+2;
			rootdir_n = begin+3;

			cerr << "Root fileset @ " << rootfileset_n << endl;
			cerr << "Root terminator @ " << rootterm_n << endl;
			cerr << "Root file entry @ " << rootfileent_n << endl;
			cerr << "Root directory @ " << rootdir_n << endl;
		}

		PartitionTagSector = VolumeDescriptorSequenceExtent->start + 2;

		UDF_tag_primary_volume_descriptor *primary =
			(UDF_tag_primary_volume_descriptor*)(data + 2048*0);
		SET_UDF_tag(primary->DescriptorTag,UDFtag_PrimaryVolumeDescriptor,
			VolumeDescriptorSequenceExtent->start + 0);
		UPDATE_UDF_tag(primary->DescriptorTag);
		LSETDWORD(&primary->VolumeDescriptorSequenceNumber,0);
		LSETDWORD(&primary->PrimaryVolumeDescriptorNumber,0);
		UDF_dstring_strncpy(primary->VolumeIdentifier,
			sizeof(primary->VolumeIdentifier),
			volume_label.c_str());
		LSETDWORD(&primary->VolumeSequenceNumber,1);
		LSETDWORD(&primary->MaximumVolumeSequenceNumber,1);
		LSETWORD(&primary->InterchangeLevel,3);
		LSETWORD(&primary->MaximumInterchangeLevel,3);
		LSETDWORD(&primary->CharacterSetList,1);
		LSETDWORD(&primary->MaximumCharacterSetList,1);
		UDF_dstring_strncpy(primary->VolumeSetIdentifier,
			sizeof(primary->VolumeSetIdentifier),
			volume_set_identifier.c_str());
		primary->DescriptorCharacterSet.CharacterSetType = 0;
		strcpy((char*)primary->DescriptorCharacterSet.CharacterSetInformation,"OSTA Compressed Unicode");
		primary->ExplanatoryCharacterSet.CharacterSetType = 0;
		strcpy((char*)primary->ExplanatoryCharacterSet.CharacterSetInformation,"OSTA Compressed Unicode");
		SET_UDF_extent_ad(primary->VolumeAbstract,0,0);
		SET_UDF_extent_ad(primary->VolumeCopyrightNotice,0,0);
		SET_UDF_regid(primary->ApplicationIdentifier,0,"*mkudfiso","");
		UDF_timestamp_set(primary->RecordingDateAndTime,time(NULL));
		SET_UDF_regid(primary->ImplementationIdentifier,0,"*mkudfiso","");
		SET_UDF_tag_checksum(primary->DescriptorTag,2);
		{
			SingleSectorGap s = {490,2047};
			single_sector_gaps[VolumeDescriptorSequenceExtent->start + 0] = s;
		}

		// TODO: What the hell is this? ECMA-167 doesn't describe this? Is this mentioned in UDF v1.02 spec?
		UDF_tag_implementation_use_volume_descriptor *impl2 =
			(UDF_tag_implementation_use_volume_descriptor*)(data + 2048*1);
		SET_UDF_tag(impl2->DescriptorTag,UDFtag_ImplementationUseVolumeDescriptor,
			VolumeDescriptorSequenceExtent->start + 1);
		UPDATE_UDF_tag(impl2->DescriptorTag);
		LSETDWORD(&impl2->VolumeDescriptorSequenceNumber,1);
		SET_UDF_regid(impl2->ImplementationIdentifier,0,"*UDF LV Info","\x02\x01\x05");
		{
			UDF_charspec* t1 = (UDF_charspec*)(((unsigned char*)impl2) + 52);
			t1->CharacterSetType = 0;
			strcpy((char*)t1->CharacterSetInformation,"OSTA Compressed Unicode");

			UDF_dstring_strncpy((((unsigned char*)impl2) + 116),84,volume_label.c_str());
		}
		SET_UDF_tag_checksum(impl2->DescriptorTag,2);
		{
			SingleSectorGap s = {512,2047};
			single_sector_gaps[VolumeDescriptorSequenceExtent->start + 1] = s;
		}

		UDF_tag_partition_descriptor *partition =
			(UDF_tag_partition_descriptor*)(data + 2048*2);
		SET_UDF_tag(partition->DescriptorTag,UDFtag_PartitionDescriptor,
			VolumeDescriptorSequenceExtent->start + 2);
		UPDATE_UDF_tag(partition->DescriptorTag);
		LSETDWORD(&partition->VolumeDescriptorSequenceNumber,2);
		LSETWORD(&partition->PartitionFlags,1);
		LSETWORD(&partition->PartitionNumber,0);
		SET_UDF_regid(partition->PartitionContents,0x02,"+NSR02","");
		LSETDWORD(&partition->AccessType,1);
		LSETDWORD(&partition->PartitionStartingLocation,(PartitionStart = rootfileset_n));
		LSETDWORD(&partition->PartitionLength,0x7FFFFFFF);	// a guess, this will be updated later
		SET_UDF_regid(partition->ImplementationIdentifier,0,"*mkudfiso","");
		SET_UDF_tag_checksum(partition->DescriptorTag,2);
		{
			SingleSectorGap s = {356,2047};
			single_sector_gaps[VolumeDescriptorSequenceExtent->start + 2] = s;
		}
		UDF_tag_logical_volume_descriptor *volume =
			(UDF_tag_logical_volume_descriptor*)(data + 2048*3);
		SET_UDF_tag(volume->DescriptorTag,UDFtag_LogicalVolumeDescriptor,
			VolumeDescriptorSequenceExtent->start + 3);
		UPDATE_UDF_tag(volume->DescriptorTag);
		LSETDWORD(&volume->VolumeDescriptorSequenceNumber,3);
		volume->DescriptorCharacterSet.CharacterSetType = 0;
		strcpy((char*)volume->DescriptorCharacterSet.CharacterSetInformation,"OSTA Compressed Unicode");
		UDF_dstring_strncpy(volume->LogicalVolumeIdentifier,
			sizeof(volume->LogicalVolumeIdentifier),
			volume_label.c_str());
		LSETDWORD(&volume->LogicalBlockSize,2048);
		SET_UDF_regid(volume->DomainIdentifier,0,"*OSTA UDF Compliant","\x02\x01\x03");
		LSETDWORD((((unsigned char*)volume) + 248),2048);
		LSETDWORD(&volume->MapTableLength,sizeof(UDF_partition_map_type1));
		LSETDWORD(&volume->NumberOfPartitionMaps,1);
		LSETDWORD(&volume->IntegritySequenceExtent.length,4096);
		LSETDWORD(&volume->IntegritySequenceExtent.location,64);
		SET_UDF_regid(volume->ImplementationIdentifier,0,"*mkudfiso","");
		UDF_partition_map_type1 *volume_pmt1 =
			(UDF_partition_map_type1*)(volume->PartitionMaps);
		volume_pmt1->PartitionMapType = 1;
		volume_pmt1->PartitionMapLength = 6;
		volume_pmt1->VolumeSequenceNumber = 0;
		volume_pmt1->PartitionNumber = 0;
		SET_UDF_tag_checksum(volume->DescriptorTag,2);
		{
			SingleSectorGap s = {512,2047};
			single_sector_gaps[VolumeDescriptorSequenceExtent->start + 3] = s;
		}

		UDF_tag_unallocated_space_descriptor *usd =
			(UDF_tag_unallocated_space_descriptor*)(data + 2048*4);
		SET_UDF_tag(usd->DescriptorTag,UDFtag_UnallocatedSpaceDescriptor,
			VolumeDescriptorSequenceExtent->start + 4);
		UPDATE_UDF_tag(usd->DescriptorTag);
		LSETDWORD(&usd->VolumeDescriptorSequenceNumber,4);
		LSETDWORD(&usd->NumberOfAllocationDescriptors,0);
		SET_UDF_tag_checksum(usd->DescriptorTag,2);
		{
			SingleSectorGap s = {512,2047};
			single_sector_gaps[VolumeDescriptorSequenceExtent->start + 4] = s;
		}

		UDF_tag_terminating_descriptor *term =
			(UDF_tag_terminating_descriptor*)(data + 2048*5);
		SET_UDF_tag(term->DescriptorTag,UDFtag_TerminatingDescriptor,
			VolumeDescriptorSequenceExtent->start + 5);
		UPDATE_UDF_tag(term->DescriptorTag);
		SET_UDF_tag_checksum(term->DescriptorTag,2);
		{
			SingleSectorGap s = {16,2047};
			single_sector_gaps[VolumeDescriptorSequenceExtent->start + 5] = s;
		}

		VolumeDescriptorSequenceExtent->setContent(data,sizeof(data));
	}
#if 0
	{
		OutputExtent *Padding = NewOutputExtent(); {
			Padding->end = 48;
		}
	}
#endif

// uncomment this if you want this software to emit a "reserve" volume descriptor.
// it depends on how much you trust your DVD-R media :)
#ifdef EMIT_RESERVE_VOLUME_DESCRIPTOR
	/* lazy way: copy what we made the first time around and reassign to an extent :) */
	OutputExtent *VolumeDescriptorSequenceExtent2 = NewOutputExtent(0,6); {
		VolumeDescriptorSequenceExtent2->setContent(VolumeDescriptorSequenceExtent->content,
			VolumeDescriptorSequenceExtent->content_length);

		UDF_tag_terminating_descriptor *t;
		int i;

		for (i=0;i < 6;i++) {
			t = (UDF_tag_terminating_descriptor*)(VolumeDescriptorSequenceExtent2->content + (i*2048));
			UDF_Uint8 tg = t->DescriptorTag.TagIdentifier;
			SET_UDF_tag(t->DescriptorTag,tg,VolumeDescriptorSequenceExtent2->start+i);
			SET_UDF_tag_checksum(t->DescriptorTag,2);
		}
	}
#endif

#if 0
	{
		OutputExtent *filler1 = NewOutputExtent(); {
			filler1->end = 64;
		}
	}
#endif

	OutputExtent *LogicalVolumeIntegrity = NewOutputExtent(0,2); {
		unsigned char data[2048*2];
		memset(data,0,2048*2);

		UDF_tag_logical_volume_integrity_descriptor *lv =
			(UDF_tag_logical_volume_integrity_descriptor*)(data + 2048*0);
		SET_UDF_tag(lv->DescriptorTag,UDFtag_LogicalVolumeIntegrityDescriptor,
			LogicalVolumeIntegrity->start);
		UPDATE_UDF_tag(lv->DescriptorTag);
		UDF_timestamp_set(lv->RecordingDateAndTime,time(NULL));
		LSETDWORD(&lv->IntegrityType,1);
		LSETDWORD(&lv->NumberOfPartitions,1);
		LSETDWORD(&lv->LengthOfImplementationUse,46);
		LSETDWORD((((unsigned char*)lv)+84),0x7FFFFFFF);
		UDF_tag_logical_volume_integrity_descriptor_implementation_use_UDFv102 *iuv102 =
			(UDF_tag_logical_volume_integrity_descriptor_implementation_use_UDFv102*)(((unsigned char*)lv)+88);
		SET_UDF_regid(iuv102->ImplementationID,0,"*mkudfiso","\x05");
		LSETDWORD(&iuv102->NumberOfFiles,999999999);
		LSETDWORD(&iuv102->NumberOfDirectories,999999999);
		LSETWORD(&iuv102->MinimumUDFReadRevision,0x0102);
		LSETWORD(&iuv102->MinimumUDFWriteRevision,0x0102);
		LSETWORD(&iuv102->MaximumUDFWriteRevision,0x0102);
		SET_UDF_tag_checksum(lv->DescriptorTag,2);
		{
			SingleSectorGap s = {512,2047};
			single_sector_gaps[LogicalVolumeIntegrity->start] = s;
		}

		UDF_tag_terminating_descriptor *term =
			(UDF_tag_terminating_descriptor*)(data + 2048*1);
		SET_UDF_tag(term->DescriptorTag,UDFtag_TerminatingDescriptor,LogicalVolumeIntegrity->start + 1);
		UPDATE_UDF_tag(term->DescriptorTag);
		SET_UDF_tag_checksum(term->DescriptorTag,2);

		LogicalVolumeIntegrity->setContent(data,sizeof(data));
		{
			SingleSectorGap s = {16,2047};
			single_sector_gaps[LogicalVolumeIntegrity->start + 1] = s;
		}
	}

#if 0
	{
		/* fill in between end of volume descriptor sequence extent ... 256 */
		OutputExtent *filler1 = NewOutputExtent(); {
			filler1->end = 256;
		}
	}
#endif

	/* generate anchor */
	OutputExtent *UDFAnchor1 = NewOutputExtent(256); {
		UDF_tag_anchor_volume_descriptor anchor;
		memset(&anchor,0,sizeof(anchor));
		SET_UDF_tag(anchor.DescriptorTag,UDFtag_AnchorVolumeDescriptor,256);
		UPDATE_UDF_tag(anchor.DescriptorTag);
		SET_UDF_extent_ad(anchor.MainVolumeDescriptorSequenceExtent,2048*16,VolumeDescriptorSequenceExtent->start);
#ifdef EMIT_RESERVE_VOLUME_DESCRIPTOR
		SET_UDF_extent_ad(anchor.ReserveVolumeDescriptorSequenceExtent,2048*16,VolumeDescriptorSequenceExtent2->start);
#else
		SET_UDF_extent_ad(anchor.ReserveVolumeDescriptorSequenceExtent,2048*16,0);
#endif
		SET_UDF_tag_checksum(anchor.DescriptorTag,2);
		/* only the first 32 bytes have relevence (32...511 are "reserved") */
		UDFAnchor1->setContent(&anchor,32); // sizeof(anchor));
	}

	// NOTE: From now on the "location" fields of the UDF tags are partition relative.
	//       This is apparently required by the Linux UDF driver.
	//       I wish ECMA and OSTA would clarify that fact in their standards documents.

	/* root fileset */
	OutputExtent *RootFileset = NewOutputExtent(rootfileset_n); {
		UDF_tag_file_set_descriptor fset;
		memset(&fset,0,sizeof(fset));
		SET_UDF_tag(fset.DescriptorTag,UDFtag_FileSetDescriptor,rootfileset_n - PartitionStart);
		UPDATE_UDF_tag(fset.DescriptorTag);
		UDF_timestamp_set(fset.RecordingDateAndTime,time(NULL));
		LSETWORD(&fset.InterchangeLevel,3);
		LSETWORD(&fset.MaximumInterchangeLevel,3);
		LSETDWORD(&fset.CharacterSetList,1);
		LSETDWORD(&fset.MaximumCharacterSetList,1);
		LSETDWORD(&fset.FileSetNumber,0);
		LSETDWORD(&fset.FileSetDescriptorNumber,0);
		fset.LogicalVolumeIdentifierCharacterSet.CharacterSetType = 0;
		strcpy((char*)fset.LogicalVolumeIdentifierCharacterSet.CharacterSetInformation,
			"OSTA Compressed Unicode");
		UDF_dstring_strncpy(fset.LogicalVolumeIdentifier,
			sizeof(fset.LogicalVolumeIdentifier),
			volume_label.c_str());
		fset.FileSetCharacterSet.CharacterSetType = 0;
		strcpy((char*)fset.FileSetCharacterSet.CharacterSetInformation,"OSTA Compressed Unicode");
		UDF_dstring_strncpy(fset.FileSetIdentifier,
			sizeof(fset.FileSetIdentifier),
			volume_label.c_str());
		UDF_dstring_strncpy(fset.CopyrightFileIdentifier,
			sizeof(fset.CopyrightFileIdentifier),
			"");
		UDF_dstring_strncpy(fset.AbstractFileIdentifier,
			sizeof(fset.AbstractFileIdentifier),
			"");
		SET_UDF_regid(fset.DomainIdentifier,0,"*OSTA UDF Compliant","\x02\x01\x03");
		fset.RootDirectoryICB.ExtentLength = 2048;
		fset.RootDirectoryICB.ExtentLocation.LogicalBlockNumber = rootfileent_n - PartitionStart;
		fset.RootDirectoryICB.ExtentLocation.PartitionReferenceNumber = 0;
		SET_UDF_tag_checksum(fset.DescriptorTag,2);
		/* bytes 480...511 are "reserved" */
		RootFileset->setContent(&fset,480); // sizeof(fset));
	}
	/* terminator */
	OutputExtent *RootFileTerm = NewOutputExtent(rootterm_n); {
		UDF_tag_terminating_descriptor t;
		memset(&t,0,sizeof(t));
		SET_UDF_tag(t.DescriptorTag,UDFtag_TerminatingDescriptor,RootFileTerm->start);
		UPDATE_UDF_tag(t.DescriptorTag);
		/* bytes 16...511 are "reserved" */
		RootFileTerm->setContent(&t,16); // sizeof(t));
	}
	/* root file entry */
	OutputExtent *RootFileEntry = NewOutputExtent(rootfileent_n); {
		UDF_tag_file_entry_descriptor fed;
		memset(&fed,0,sizeof(fed));
		SET_UDF_tag(fed.DescriptorTag,UDFtag_FileEntry,rootfileent_n - PartitionStart);
		UPDATE_UDF_tag(fed.DescriptorTag);
		fed.ICBTag.FileType = 4;	// directory
		fed.ICBTag.Flags = 0x0230;	// don't relocate this, use short_ad
		fed.ICBTag.StrategyType = 4;
		LSETWORD(&fed.ICBTag.StrategyParameter,0);
		fed.ICBTag.MaximumNumberOfEntries = 1;
		fed.Uid = -1;
		fed.Gid = -1;
		fed.Permissions = 0x14A5;
		fed.FileLinkCount = 1;
		fed.InformationLength = 0;// TODO: Length of the root directory
		fed.LogicalBlocksRecorded = 0;// TODO: Length of the root directory
		UDF_timestamp_set(fed.AccessDateAndTime,time(NULL));
		UDF_timestamp_set(fed.ModificationDateAndTime,time(NULL));
		UDF_timestamp_set(fed.AttributeDateAndTime,time(NULL));
		fed.Checkpoint = 1;
		SET_UDF_regid(fed.ImplementationIdentifier,0,"*mkudfiso","");
		fed.UniqueId = 0;
		fed.LengthOfAllocationDescriptors = sizeof(UDF_short_ad);
		UDF_short_ad *ex = (UDF_short_ad*)(((unsigned char*)(&fed)) + 176);
		ex->ExtentLength = 0;// TODO: Length of root directory
		ex->ExtentPosition = rootdir_n - PartitionStart;
		SET_UDF_tag_checksum(fed.DescriptorTag,2);
		RootFileEntry->setContent(&fed,sizeof(fed));
	}
	UDF_tag_file_entry_descriptor *RootFileEntryTag =
		(UDF_tag_file_entry_descriptor*)(RootFileEntry->content);
	UDF_short_ad *RootFileEntryTagExtent =
		(UDF_short_ad*)(((unsigned char*)(RootFileEntryTag)) + 176);

	/* now generate the root directory */
	OutputExtent *RootDirectory = NULL; {
		map<UDF_Uint64,FileEntry>::iterator i;
		FileEntry *oex = NULL;
		int alloc_sz = 40;	/* room for . and .. */

/* how much memory/space do we need to create the directory? */
		for (	i  = file_list.find(parent_dir_to_first_file[0]);
			i != file_list.end() && i->second.parent == 0;i++) {
			oex = &i->second;
			int sz =
				16 + 2 + 1 + 1 + 16 + 2 +
				0 + /* Length of Implementation Use */
				/* null-length Implementation Use */
				oex->name.length() + 1;	/* File Identifier (as a d-string) */
			sz = (sz + 3) & (~3);		/* padding (up to next DWORD) */
			alloc_sz += sz;
		}
		if (alloc_sz == 0) {
			cerr << "ERROR: Program bug: Apparently there is no root directory" << endl;
			return 1;
		}

/* create the directory */
		RootDirectory = NewOutputExtent(rootdir_n,(alloc_sz+2047) >> 11);
		unsigned char *dir_raw = new unsigned char[alloc_sz];
		unsigned char *dir_cur = dir_raw;
		memset(dir_raw,0,alloc_sz);

		/* . and .. */
		{
			UDF_tag_file_identifier_descriptor *fent =
				(UDF_tag_file_identifier_descriptor*)dir_cur;
			SET_UDF_tag(fent->DescriptorTag,UDFtag_FileIdentifierDescriptor,
				RootFileEntryTagExtent->ExtentPosition);
			UPDATE_UDF_tag(fent->DescriptorTag);
			fent->FileVersionNumber = 1;
			fent->FileCharacteristics = 0x0A;	/* parent node */
			fent->LengthOfFileIdentifier = 0;
			fent->ICB.ExtentLength = 2048;
			fent->ICB.ExtentLocation.LogicalBlockNumber = rootfileent_n - PartitionStart;	/* ourself */
			fent->ICB.ExtentLocation.PartitionReferenceNumber = 0;
			SET_UDF_tag_checksum(fent->DescriptorTag,2);
			dir_cur += 40;
		}

		list< pair<UDF_Uint64,FileEntry*> > dir_ents;
		list< pair<UDF_Uint64,FileEntry*> > file_ents;
		for (	i  = file_list.find(parent_dir_to_first_file[0]);
			i != file_list.end() && i->second.parent == 0;i++) {
			oex = &i->second;
			int sz =
				16 + 2 + 1 + 1 + 16 + 2 +
				0 + /* Length of Implementation Use */
				/* null-length Implementation Use */
				oex->name.length() + 1;	/* File Identifier (as a d-string) */
			sz = (sz + 3) & (~3);		/* padding (up to next DWORD) */

			if ((dir_cur + sz) > (dir_raw + alloc_sz)) {
				cerr << "ERROR: Program bug: Miscalculation of an entry in the root dir" << endl;
				return 1;
			}

			/* TODO: For files larger than 231GB, multiple sectors are needed for the allocation extent array.
			 *       Note that the current CD/DVD/Bluray/HD-DVD media is not that large, so this is not a concern yet */
			OutputExtent *FileEntry2 = NewOutputExtent();
			unsigned char FileEntry2TagRaw[2048];
			memset(FileEntry2TagRaw,0,sizeof(FileEntry2TagRaw));
			UDF_tag_file_entry_descriptor &FileEntry2Tag = *((UDF_tag_file_entry_descriptor*)FileEntry2TagRaw);
			SET_UDF_tag(FileEntry2Tag.DescriptorTag,UDFtag_FileEntry,
				FileEntry2->start - PartitionStart);
			UPDATE_UDF_tag(FileEntry2Tag.DescriptorTag);
			FileEntry2Tag.ICBTag.PriorRecordedNumberOfDirectEntries = 0;
			FileEntry2Tag.ICBTag.StrategyType = 4;
			LSETWORD(&FileEntry2Tag.ICBTag.StrategyParameter,0);
			FileEntry2Tag.ICBTag.MaximumNumberOfEntries = 1;
			FileEntry2Tag.ICBTag.FileType = (oex->characteristics & 2) ? 4 : 5; /* file or folder? */
			FileEntry2Tag.ICBTag.ParentICBLocation.LogicalBlockNumber = rootfileent_n - PartitionStart;
			FileEntry2Tag.ICBTag.ParentICBLocation.PartitionReferenceNumber = 0;
			FileEntry2Tag.ICBTag.Flags = 0x230;	/* non-relocatable, short_ad */
			FileEntry2Tag.Uid = -1;
			FileEntry2Tag.Gid = -1;
			FileEntry2Tag.Permissions = oex->permissions;
			FileEntry2Tag.FileLinkCount = 1;
			FileEntry2Tag.RecordFormat = 0;
			FileEntry2Tag.RecordDisplayAttributes = 0;
			FileEntry2Tag.RecordLength = 0;
			FileEntry2Tag.InformationLength = oex->file_size;
			FileEntry2Tag.LogicalBlocksRecorded = (oex->file_size + 2047LL) >> 11LL;
			FileEntry2Tag.AccessDateAndTime = oex->file_atime;
			FileEntry2Tag.AttributeDateAndTime = oex->file_ctime;
			FileEntry2Tag.ModificationDateAndTime = oex->file_mtime;
			FileEntry2Tag.Checkpoint = 1;
			FileEntry2Tag.UniqueId = oex->id;
			SET_UDF_regid(FileEntry2Tag.ImplementationIdentifier,0,"*mkudfiso","");
			/* set aside directories for later */
			if (oex->characteristics & 2) {
				/* add to list */
				pair<UDF_Uint64,FileEntry*> po(FileEntry2->start,&file_list[i->first]);
				dir_ents.push_back(po);
			}
			/* if the file is small enough we can stick the contents directly INTO the descriptor
			 * where the allocation extents normally go. See ECMA-167 4/14.6 "ICB tag" */
			else if (FileEntry2Tag.InformationLength < (2048-176)) {
				FileEntry2Tag.ICBTag.Flags = 0x233;	/* non-relocateable, the allocation extent area IS the file content */
				FileEntry2Tag.LengthOfAllocationDescriptors = FileEntry2Tag.InformationLength;
				FileEntry2Tag.LogicalBlocksRecorded = 0;

				if (extra_large_chdir(oex->path.c_str()) < 0) {
					cerr << "Cannot enter " << oex->path << endl;
				}
				else {
					int fd = open(oex->name.c_str(),O_RDONLY);
					if (fd < 0) {
						cerr << "Cannot open " << oex->abspath << endl;
					}
					else {
						int r = read(fd,((unsigned char*)(&FileEntry2Tag))+176,FileEntry2Tag.InformationLength);
						if (r < FileEntry2Tag.InformationLength)
							cerr << "WARNING: Read less data than expected for " << oex->abspath << endl;
						close(fd);
					}
				}
			}
			else {
				/* add to list */
				pair<UDF_Uint64,FileEntry*> po(FileEntry2->start,&file_list[i->first]);
				file_ents.push_back(po);
			}
			SET_UDF_tag_checksum(FileEntry2Tag.DescriptorTag,2);
			FileEntry2->setContent(&FileEntry2Tag,2048);

			/* create the directory entry */
			UDF_tag_file_identifier_descriptor *fent =
				(UDF_tag_file_identifier_descriptor*)dir_cur;
			SET_UDF_tag(fent->DescriptorTag,UDFtag_FileIdentifierDescriptor,
				RootFileEntryTagExtent->ExtentPosition);
			UPDATE_UDF_tag(fent->DescriptorTag);
			fent->FileVersionNumber = 1;
			fent->FileCharacteristics = oex->characteristics;
			fent->LengthOfFileIdentifier = oex->name.length()+1;	/* doesn't count d-string type? */
			fent->ICB.ExtentLength = 2048;
			fent->ICB.ExtentLocation.LogicalBlockNumber = FileEntry2->start - PartitionStart;
			fent->ICB.ExtentLocation.PartitionReferenceNumber = 0;
			UDF_dstring_strncpyne((dir_cur+38),(oex->name.length()+1),oex->name.c_str());
			SET_UDF_tag_checksum(fent->DescriptorTag,2);

			/* advance */
			dir_cur += sz;
		}
		RootDirectory->setContent(dir_raw,alloc_sz);
		delete dir_raw;

		RootFileEntryTag->InformationLength = alloc_sz;
		RootFileEntryTag->LogicalBlocksRecorded = (alloc_sz + 2047LL) >> 11LL;
		RootFileEntryTagExtent->ExtentLength = alloc_sz;
		SET_UDF_tag_checksum(RootFileEntryTag->DescriptorTag,2);

		pair<UDF_Uint64,FileEntry*> prpair;
		list< pair<UDF_Uint64,FileEntry*> >::iterator pri;

		/* put folders in */
		for (pri=dir_ents.begin();pri != dir_ents.end();pri++) {
			UDF_Uint64 sector = pri->first;
			FileEntry* file = pri->second;
			OutputExtent *sx = &output_extents[sector];
			UDF_subdirectory(RootFileEntryTagExtent,RootDirectory,file->id,sx);
		}
		dir_ents.clear();

		/* put files in */
		for (pri=file_ents.begin();pri != file_ents.end();pri++) {
			UDF_Uint64 sector = pri->first;
			FileEntry* file = pri->second;
			OutputExtent *sx = &output_extents[sector];
			if (!sx->content) {
				cerr << "Unexpected: Sector " << sector << " does not have contents" << endl;
				continue;
			}

			UDF_tag_file_entry_descriptor *fed =
				(UDF_tag_file_entry_descriptor*)(sx->content);

			int allocs_needed = 1;
			UDF_Uint64 ssz = fed->InformationLength;
			UDF_short_ad *sad = (UDF_short_ad*)(sx->content + 176);

			/* how many allocation extents */
			while (ssz >= 1048576000) {
				ssz -= 1048576000;
				allocs_needed++;
			}

			fed->LengthOfAllocationDescriptors =
				allocs_needed * 8;

			/* make the output extent for the file */
			OutputExtent *fex = NewOutputExtent(0,fed->LogicalBlocksRecorded);

			/* build extents */
			{
				UDF_Uint32 s = fex->start - PartitionStart;
				ssz = fed->InformationLength;
				while (ssz >= 1048576000) {
					LSETDWORD(&sad->ExtentLength,1048576000);
					LSETDWORD(&sad->ExtentPosition,s);
					sad = (UDF_short_ad*)(((unsigned char*)sad) + 8);
					s += 1048576000 >> 11;
					ssz -= 1048576000;
				}
				if (ssz > 0) {
					LSETDWORD(&sad->ExtentLength,((UDF_Uint32)ssz));
					LSETDWORD(&sad->ExtentPosition,s);
					sad = (UDF_short_ad*)(((unsigned char*)sad) + 8);
					s += (ssz + 2047) >> 11;
					ssz = 0;
				}
				if ((s+PartitionStart) != fex->end) {
					cerr << "BUG: Extent computation ended at " << s << " expected " << fex->end << endl;
				}
				if (allocs_needed != ((((unsigned char*)sad) - (sx->content + 176))>>3)) {
					cerr << "BUG: Miscalculated the number of extents" << endl;
				}
			}

			fex->setFile(file);
			SET_UDF_tag_checksum(fed->DescriptorTag,sx->content_length-16);
		}
		file_ents.clear();
	}

	UDF_Uint64 highest_sector;
	/* total ISO size? */
	{
		map<UDF_Uint64,OutputExtent>::reverse_iterator ri = output_extents.rbegin();
		highest_sector = ri->second.end;
		cout << "Total ISO size: " << humanize(highest_sector << 11LL) <<
			", or " << highest_sector << " sectors" << endl;
	}

	/* update the partition descriptor */
	{
		unsigned char *ptr = output_extents[VolumeDescriptorSequenceExtent->start].content;
		int sz = output_extents[VolumeDescriptorSequenceExtent->start].content_length;
		if (ptr && sz > (2048*3)) {
			UDF_tag_partition_descriptor *p = (UDF_tag_partition_descriptor*)(ptr + 2048*2);
			LSETDWORD(&p->PartitionLength,highest_sector - PartitionStart);
			SET_UDF_tag_checksum(p->DescriptorTag,2);
		}
	}

	unsigned char iso_sha256[32];
	unsigned char iso_sha1[20];
	unsigned char iso_md5[16];
	UDF_Uint64 iso_sectors = 0;
	int iso_fd = 1;	// STDOUT by default

	if (iso_file != "") {
		/* default behavior is to NOT overwrite the existing file, unless forced */
		if (iso_overwrite)
			iso_fd = open64(iso_file.c_str(),O_RDWR | O_CREAT | O_TRUNC,0644);
		else
			iso_fd = open64(iso_file.c_str(),O_RDWR | O_CREAT | O_EXCL,0644);

		if (iso_fd < 0) {
			cerr << "Cannot create ISO file " << strerror(errno) << endl;
			return 1;
		}

		if (volume_label == "") {
			const char *c = strrchr(iso_file.c_str(),'/');
			if (c) c++;
			else c = iso_file.c_str();

			int l = strlen(c);
			if (l > 32) l = 32;

			{
				const char *d = strrchr(c,'.');
				if (d) {
					int p = (int)(d-c);
					if (l > p) l = p;
				}
			}

			if (l > 0)
				volume_label = string(c,l);
		}
	}

	/* generate report file, if requested */
	if (report_file != "") {
		FILE *rfp = fopen(report_file.c_str(),"wb");
		if (!rfp) {
			cerr << "Cannot generate report file" << endl;
			return 1;
		}

		time_t t = time(NULL);
		fprintf(rfp,"mkudfiso report for volume \"%s\" volumeset \"%s\"\n",
			volume_label.c_str(),
			volume_set_identifier.c_str());
		fprintf(rfp,"Generated %s\n",ctime(&t));	/* one empty line: ctime() makes it's own \n */

		{
			map<UDF_Uint64,OutputExtent>::iterator i = output_extents.begin();
			while (i != output_extents.end()) {
				if (i->second.file) {
					fprintf(rfp,"Entry %s\n",i->second.file->name.c_str());
					fprintf(rfp,"\t" "Absolute path: %s\n",i->second.file->abspath.c_str());
					fprintf(rfp,"\t" "File size: %Lu\n",i->second.file->file_size);
					fprintf(rfp,"\t" "Sectors: %Lu-%Lu\n",i->second.start,i->second.end-1LL);
					fprintf(rfp,"\n");
				}

				i++;
			}
		}

		fclose(rfp);
	}

	/* attach report file to the end of the ISO */
	if (report_file != "") {
		signed long long report_sz = 0;
		{
			int fd = open64(report_file.c_str(),O_RDONLY);
			report_sz = lseek64(fd,0,SEEK_END);
			close(fd);
		}

		/* if space allows, append the report as an Implementation Specific UDF tag */
		if (iso_size_limit > 0 && (highest_sector << 11LL) + report_sz + 4096 > iso_size_limit) {
			cerr << "Not inserting report into ISO, not enough space" << endl;
		}
		else {
			UDF_Uint64 fst = file_list_alloc();
			FileEntry *fs = &file_list[fst];
			fs->parent = -1;
			fs->file_size = report_sz;
			fs->path = report_file;
			fs->abspath = report_file;
			fs->name = "report";

			OutputExtent *fsx = NewOutputExtent(0,(report_sz + 2047LL) >> 11LL);
			fsx->setFile(fs);

			OutputExtent *rtx = NewOutputExtent();
			UDF_tag_implementation_use_volume_descriptor ftag;
			memset(&ftag,0,sizeof(ftag));
			SET_UDF_tag(ftag.DescriptorTag,UDFtag_ImplementationUseVolumeDescriptor,rtx->start);
			UPDATE_UDF_tag(ftag.DescriptorTag);
			LSETDWORD(&ftag.VolumeDescriptorSequenceNumber,1);
			SET_UDF_regid(ftag.ImplementationIdentifier,1,"*mkudfiso","Report");
			LSETDWORD((((unsigned char*)(&ftag))+52),fsx->start);
			LSETDWORD((((unsigned char*)(&ftag))+56),report_sz);
			SET_UDF_tag_checksum(ftag.DescriptorTag,60-16);
			rtx->setContent(&ftag,64); // sizeof(ftag));
		}
	}

	/* generate ISO */
	{
		map<UDF_Uint64,OutputExtent>::iterator i = output_extents.begin();
		char do_hash=(hashtable_file.length() > 0) ? 1 : 0;
		unsigned char sectorbuffer[2048];
		UDF_Uint64 n=0,max=highest_sector;

		sha256_context sha256_ctx;
		sha1_context sha1_ctx;
		md5_context md5_ctx;

		if (do_hash) {
			sha256_starts(&sha256_ctx);
			sha1_starts(&sha1_ctx);
			md5_starts(&md5_ctx);
		}

		while (i != output_extents.end()) {
			if (n < i->second.start) {
				memset(sectorbuffer,0,2048);
				while (n < i->second.start) {
					if (do_hash) {
						sha256_update(&sha256_ctx,sectorbuffer,2048);
						sha1_update(&sha1_ctx,sectorbuffer,2048);
						md5_update(&md5_ctx,sectorbuffer,2048);
						iso_sectors++;
					}

					write(iso_fd,sectorbuffer,2048);
					n++;
				}
			}

			if (i->second.file) {
				if (isatty(1)) {
					cout << "        writing: " <<
						i->second.file->name << " (" << humanize(i->second.file->file_size) <<
						" of the " << humanize(highest_sector << 11ll) << " iso)" << endl;
				}

				UDF_Uint64 hash_len=0;
				FileEntry *f = i->second.file;

				if (do_hash) {
					sha256_starts(&f->sha256_ctx);
					sha1_starts(&f->sha1_ctx);
					md5_starts(&f->md5_ctx);
				}

				int in_fd = open64(f->abspath.c_str(),O_RDONLY);
				if (in_fd >= 0) {
					UDF_Uint64 cp = 0;
					lseek64(in_fd,0,SEEK_SET);
					while (n < i->second.end) {
						int rd = read(in_fd,sectorbuffer,2048);
						if (rd < 0) rd = 0;
						if (rd < 2048) memset(sectorbuffer+rd,0,2048-rd);
						if (write(iso_fd,sectorbuffer,2048) < 2048) {
							fprintf(stderr,"write error: cannot write iso image. %s\n",strerror(errno));
							exit(1);
						}
						cp += rd;
						n++;

						if (do_hash) {
							sha256_update(&sha256_ctx,sectorbuffer,2048);
							sha1_update(&sha1_ctx,sectorbuffer,2048);
							md5_update(&md5_ctx,sectorbuffer,2048);
							iso_sectors++;
						}

						if (do_hash && rd > 0) {
							sha256_update(&f->sha256_ctx,sectorbuffer,rd);
							sha1_update(&f->sha1_ctx,sectorbuffer,rd);
							md5_update(&f->md5_ctx,sectorbuffer,rd);
							hash_len += rd;
						}
					}
					close(in_fd);

					if (cp != f->file_size) {
						cerr << "error: i read in " << cp <<
							" bytes when the file was reported as " <<
							f->file_size << " bytes." << endl;
						return 1;
					}
				}
				else {
					cerr << "cannot open file " << f->abspath;
					return 1;
				}

				if (do_hash) {
					f->hash_length = hash_len;
					sha256_finish(&f->sha256_ctx,f->sha256);
					sha1_finish(&f->sha1_ctx,f->sha1);
					md5_finish(&f->md5_ctx,f->md5);
				}
			}
			else if (i->second.content) {
				int o = 0;
				unsigned char *p = i->second.content;
				while (n < i->second.end) {
					int rem = i->second.content_length - o;
					if (rem > 2048) rem = 2048;
					n++;

					if (rem < 2048)
						memset(sectorbuffer+rem,0,2048-rem);

					if (rem > 0) {
						memcpy(sectorbuffer,p,rem);
						o += rem;
						p += rem;
					}

					if (write(iso_fd,sectorbuffer,2048) < 2048) {
						fprintf(stderr,"write error: cannot write iso image. %s\n",strerror(errno));
						exit(1);
					}

					if (do_hash) {
						sha256_update(&sha256_ctx,sectorbuffer,2048);
						sha1_update(&sha1_ctx,sectorbuffer,2048);
						md5_update(&md5_ctx,sectorbuffer,2048);
						iso_sectors++;
					}
				}
			}

			/* next item */
			i++;
		}

		if (do_hash) {
			sha256_finish(&sha256_ctx,iso_sha256);
			sha1_finish(&sha1_ctx,iso_sha1);
			md5_finish(&md5_ctx,iso_md5);
		}
	}

	/* generate hash file, if requested */
	if (hashtable_file != "") {
		unsigned char sectorbuffer[2048];
		int rd;

		FILE *rfp = fopen(hashtable_file.c_str(),"wb");
		if (!rfp) {
			cerr << "Cannot generate hash file" << endl;
			return 1;
		}

		time_t t = time(NULL);
		fprintf(rfp,"mkudfiso hash table for volume \"%s\" volumeset \"%s\"\n",
			volume_label.c_str(),
			volume_set_identifier.c_str());
		fprintf(rfp,"Generated %s\n",ctime(&t));	/* one empty line: ctime() makes it's own \n */

		{
			map<UDF_Uint64,OutputExtent>::iterator i = output_extents.begin();
			while (i != output_extents.end()) {
				if (i->second.file) {
					fprintf(rfp,"Entry %s\n",i->second.file->name.c_str());
					fprintf(rfp,"\t" "Absolute path: %s\n",i->second.file->abspath.c_str());
					fprintf(rfp,"\t" "Hash length: %Lu\n",i->second.file->hash_length);
					fprintf(rfp,"\t" "Sectors: %Lu-%Lu\n",i->second.start,i->second.end-1LL);

					fprintf(rfp,"\t" "MD5/SHA-1/SHA-256: ");
#define C "%02x"
#define C4 C C C C
#define D4(a,x) a[x],a[x+1],a[x+2],a[x+3]
					fprintf(rfp,C4 C4 C4 C4 "/",
						D4(i->second.file->md5,0),D4(i->second.file->md5,4),
						D4(i->second.file->md5,8),D4(i->second.file->md5,12));
					fprintf(rfp,C4 C4 C4 C4 C4 "/",
						D4(i->second.file->sha1,0),D4(i->second.file->sha1,4),
						D4(i->second.file->sha1,8),D4(i->second.file->sha1,12),
						D4(i->second.file->sha1,16));
					fprintf(rfp,C4 C4 C4 C4 C4 C4 C4 C4 "\n",
						D4(i->second.file->sha256,0),D4(i->second.file->sha256,4),
						D4(i->second.file->sha256,8),D4(i->second.file->sha256,12),
						D4(i->second.file->sha256,16),D4(i->second.file->sha256,20),
						D4(i->second.file->sha256,24),D4(i->second.file->sha256,28));
#undef C4
#undef C
					fprintf(rfp,"\n");
				}

				i++;
			}
		}

		fprintf(rfp,"Whole ISO information:\n");
		fprintf(rfp,"\t" "Sectors:           %Lu\n",iso_sectors);
		fprintf(rfp,"\t" "MD5/SHA-1/SHA-256: ");
#define C "%02x"
#define C4 C C C C
#define D4(a,x) a[x],a[x+1],a[x+2],a[x+3]
		fprintf(rfp,C4 C4 C4 C4 "/",
				D4(iso_md5,0),D4(iso_md5,4),
				D4(iso_md5,8),D4(iso_md5,12));
		fprintf(rfp,C4 C4 C4 C4 C4 "/",
				D4(iso_sha1,0),D4(iso_sha1,4),
				D4(iso_sha1,8),D4(iso_sha1,12),
				D4(iso_sha1,16));
		fprintf(rfp,C4 C4 C4 C4 C4 C4 C4 C4 "\n",
				D4(iso_sha256,0),D4(iso_sha256,4),
				D4(iso_sha256,8),D4(iso_sha256,12),
				D4(iso_sha256,16),D4(iso_sha256,20),
				D4(iso_sha256,24),D4(iso_sha256,28));
#undef C4
#undef C
		fprintf(rfp,"\n");
		fclose(rfp);

		signed long long report_sz = 0;
		{
			int fd = open64(hashtable_file.c_str(),O_RDONLY);
			report_sz = lseek64(fd,0,SEEK_END);
			close(fd);
		}

		/* if space allows, append the report as an Implementation Specific UDF tag */
		if (iso_size_limit > 0 && (highest_sector << 11LL) + report_sz + 4096 > iso_size_limit) {
			cerr << "Not inserting report into ISO, not enough space" << endl;
		}
		else {
			UDF_Uint64 starting_sector = ((unsigned long long)lseek64(iso_fd,0,SEEK_END)) >> 11ULL;
			/* append it to the end of the ISO */
			{
				int fd = open64(hashtable_file.c_str(),O_RDONLY);
				int rd;
				while ((rd=read(fd,sectorbuffer,2048)) > 0) {
					if (rd < 2048) memset(sectorbuffer+rd,0,2048-rd);
					write(iso_fd,sectorbuffer,2048);
				}
				close(fd);
			}

			UDF_Uint64 descriptor_sector = ((unsigned long long)lseek64(iso_fd,0,SEEK_END)) >> 11ULL;
			/* add a descriptor so that finding the report is easy */
			{
				UDF_tag_implementation_use_volume_descriptor ftag;
				memset(&ftag,0,sizeof(ftag));
				SET_UDF_tag(ftag.DescriptorTag,UDFtag_ImplementationUseVolumeDescriptor,descriptor_sector);
				UPDATE_UDF_tag(ftag.DescriptorTag);
				LSETDWORD(&ftag.VolumeDescriptorSequenceNumber,1);
				SET_UDF_regid(ftag.ImplementationIdentifier,1,"*mkudfiso","Hashtbl");
				LSETDWORD((((unsigned char*)(&ftag))+52),starting_sector);
				LSETDWORD((((unsigned char*)(&ftag))+56),report_sz);
				SET_UDF_tag_checksum(ftag.DescriptorTag,60-16);
				memset(sectorbuffer,0,2048);
				memcpy(sectorbuffer,&ftag,60);
				write(iso_fd,sectorbuffer,2048);
			}

			/* add output extent for reference only, since we already copied it in and we're past the main loop */
			UDF_Uint64 fst = file_list_alloc();
			FileEntry *fs = &file_list[fst];
			fs->parent = -1;
			fs->file_size = report_sz;
			fs->path = hashtable_file;
			fs->abspath = hashtable_file;
			fs->name = "hashes";

			OutputExtent *fsx = NewOutputExtent(starting_sector,(report_sz + 2047LL) >> 11LL);
			fsx->setFile(fs);

			/* dummy reference to descriptor */
			{
				char dummy[64];
				OutputExtent *dsx = NewOutputExtent(descriptor_sector);
				dsx->setContent(dummy,64);
			}
		}
	}

	/* generate gap report file */
	if (gap_file != "") {
		map<UDF_Uint64,OutputExtent>::iterator i = output_extents.begin();
		UDF_Uint64 n=0,max=highest_sector;

		FILE *gapfp = fopen(gap_file.c_str(),"wb");
		if (!gapfp) {
			fprintf(stderr,"Cannot open %s, cannot write gap file\n",gapfp);
			return 1;
		}
		fprintf(gapfp,"# mkudfiso gap list\n");

		while (i != output_extents.end()) {
			if (n < i->second.start) {
				/* record formats:
				 *     
				 *     A                        (sector A is a gap)
				 *     A B                      (sector A <= x <= B is a gap)
				 *     (A,a-b)                  (sector A byte range a <= xb <= b is a gap)
				 *
				 */

				if ((n+1) == i->second.start)
					fprintf(gapfp,"%Lu\n",n);
				else
					fprintf(gapfp,"%Lu %Lu\n",n,i->second.start-1);

				n = i->second.start;
			}

			if (i->second.file) {
				FileEntry *f = i->second.file;
				UDF_Uint64 end = i->second.start + (f->file_size >> 11LL);
				unsigned int esb = ((unsigned int)f->file_size)&0x7FF;

				if (esb != 0) {
					fprintf(gapfp,"(%Lu,%u-2047)\n",end,esb);
					end++;
				}

				n = end;
			}
			else if (i->second.content) {
				UDF_Uint64 end = i->second.start + (i->second.content_length >> 11LL);
				unsigned int esb = ((unsigned int)i->second.content_length)&0x7FF;

				if (esb != 0) {
					fprintf(gapfp,"(%Lu,%u-2047)\n",end,esb);
					end++;
				}

				n = end;
			}

			/* next item */
			i++;
		}

		map<UDF_Uint64,SingleSectorGap>::iterator ssgi = single_sector_gaps.begin();
		while (ssgi != single_sector_gaps.end()) {
			SingleSectorGap *ssg = &ssgi->second;
			UDF_Uint64 sector = ssgi->first;

			if (ssg->start == 0 && ssg->end == 2047)
				fprintf(gapfp,"%Lu\n",sector);
			else
				fprintf(gapfp,"(%Lu,%u-%u)\n",sector,ssg->start,ssg->end);

			ssgi++;
		}

		fclose(gapfp);
	}

	if (iso_fd != 1)
		close(iso_fd);

	return 0;
}

