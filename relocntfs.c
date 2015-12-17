/*
relocntfs - deals with braindeadness with moving NTFS filesystems.

Copyright (C) 2006  Daniel J. Grace

I don't claim any major knowledge of the NTFS filesystem.
This program is merely an implementation of the process documented at
Michael Dominok's website: <http://www.dominok.net/en/it/en.it.clonexp.html>

Like any other program that tinkers with the contents of your HD, this
program may cause data corruption. USE AT YOUR OWN RISK. You have been warned.



This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <linux/hdreg.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

unsigned long fliplong(unsigned long v) {
	/* Flip a long value -- if the architecture depends on it */
	unsigned long rv;
	char buf[4], t;
	int iter;

	/* Determine system architecture */
	rv = 1;
	memcpy(&buf, &rv, 1);
	
	if(buf[0]) return v;


	/* If we reach here, byte-swapping is neccessary */
	memcpy(&buf, &v, 4);
	for(iter = 0 ; iter < 2 ; ++iter) {
		t = buf[iter];
		buf[iter] = buf[3-iter];
		buf[3-iter] = t;
	}
	memcpy(&rv, &buf, 4);

	return rv;
}

int usage(char *progname) {
	fprintf(stderr, 
		"ntfsreloc - adjust filesystem start sector of an NTFS partition"
		"\nUsage: %s [-s start] [-w] [-b] [-f] device"
		"\nwhere device points to an NTFS partition"
		"\n"
		"\nOptions:"
		"\n-w\n\tWrite new start sector to the partition."
		"\n-s start\n\tNew start sector to write.  If omitted, determined via ioctl."
		"\n\tMust be specified if -b option is used."
		"\n-b\n\tProceed even if the specified device is not a partition (e.g. a"
		"\n-b\n\tregular file)"
		"\n-f\n\tForce the operation to occur even if device does not look like a valid"
		"\n\tNTFS partition."
		"\n"
		"\nThis utility displays the current starting sector as defined by the"
		"\nthe filesystem.  No change will actually be made without the -w"
		"\noption."
		"\n"
		"\nExit status is 2 if an error occured, 1 if a change was made or is needed"
		"\nor 0 if the filesystem already has the correct values."
		"\n", progname
	);
	return 0;
}

char *optDeviceName = NULL;
int device = 0;

char optSpecifyStartSector = 0, optWrite = 0, optPrint = 0, optBlock = 0, optForce = 0;

unsigned long optStartSector = 0, geomStartSector = 0, useStartSector = 0, fsStartSector = 0;

char haveGeomStartSector = 0;

int main(int argc, char **argv) {
	int i;
	int readopts = 1;

	if(argc <= 1) {
		usage(argv[0]);
	}

	for(i = 1 ; i < argc ; ++i) {
		if(argv[i][0] == '-' && readopts) { 
			/* -s is special */
			if(argv[i][1] == 's') {
				optSpecifyStartSector = 1;

				char *sizePtr, *endPtr;
				if(argv[i][2]) {
					sizePtr = &argv[i][2];
				} else if(i+1 < argc) {
					sizePtr = argv[++i];
				} else {
					fprintf(stderr, "ERROR: Size must be specified for option -s\n");
					usage(argv[0]);
					return 1;
				}

				optStartSector = strtoul(sizePtr, &endPtr, 10);
				if(endPtr == sizePtr || *endPtr) {
					fprintf(stderr, "ERROR: Invalid size specified for option -s\n");
					usage(argv[0]);
					return 1;
				}
				continue;
			}

			if(argv[i][1] && argv[i][2]) {
				fprintf(stderr, "Unknown option '%s'\n", argv[i]);
				usage(argv[0]);
				return 1;
			}

			switch(argv[i][1]) {
				case '-': readopts = 0; break;
				case 'w': optWrite = 1; break;
				case 'p': optPrint = 1; break;
				case 'f': optForce = 1; break;
				case 'b': optBlock = 1; break;
				default:
					fprintf(stderr, "Unknown option '%s'\n", argv[i]);
					usage(argv[0]);
					return 1;
			}
			continue;
		}

		/* If we reach here, we're reading a device name */
		if(optDeviceName) {
			fprintf(stderr, "Only one device may be specified\n");
			usage(argv[0]);
			return 1;
		}
		optDeviceName = argv[i];
	}

	if(!optDeviceName) {
		fprintf(stderr, "No device name specified\n");
		usage(argv[0]);
		return 1;
	}

	/* If we reach this point, we can actually do work */

	/* Verify that we can open the device in readonly mode */
	if(!(device = open(optDeviceName, (optWrite ? O_RDWR : O_RDONLY) | O_SYNC))) {
		perror("open");
		return 2;
	}

	/* Check to see if it's a partition */
	struct hd_geometry geom;
	if(ioctl(device, HDIO_GETGEO, &geom)) {
		if(!optBlock) {
			fprintf(stderr, "Failed to read disk geometry.  Perhaps this is not a partition?\n");
			fprintf(stderr, "Verify that you are using the correct device or use the -b option.\n");
			fprintf(stderr, "The exact error was:\n");
			perror("ioctl");
			return 2;
		} else if(!optSpecifyStartSector && optWrite) {
			fprintf(stderr, "Failed to read disk geometry, and -s option was not specified.\n");
			fprintf(stderr, "No update can be made without this information.\n");
			fprintf(stderr, "The exact error was:\n");
			perror("ioctl");
			return 2;
		}			
	} else {
		geomStartSector = geom.start;
		haveGeomStartSector = 1;

		if(!optForce && !geomStartSector) {
			fprintf(stderr, "This looks like an entire disk (start=0) instead of a single partition.\n");
			fprintf(stderr, "I won't modify this without the -f (force) option.\n");
			if(optWrite) {
				return 2;
			}
		}
	}

	/* Determine if it's an NTFS partition or not */
	if(lseek(device, 3L, SEEK_SET) < 0) {
		perror("lseek");
		return 2;
	}

	/* Read "NTFS" magic, or at least what should be */
	char ntfsMagic[4];
	if(read(device, &ntfsMagic, 4) != 4 || memcmp(ntfsMagic, "NTFS", 4)) {
		if(!optForce) {
			fprintf(stderr, "This device does not appear to be a real NTFS volume.\n");
			if(!optWrite) {
				return 2;
			}
		}
	}

	/* Determine partition's start sector */
	if(lseek(device, 28L, SEEK_SET) < 0) {
		perror("lseek");
		return 2;
	}

	if(read(device, &fsStartSector, 4) != 4) {
		fprintf(stderr, "Unable to read filesystem start sector.\n");
		return 2;
	}

	fsStartSector = fliplong(fsStartSector);

	printf("NTFS Start Sector:\n");
	if(haveGeomStartSector) {
		printf("partition=%lu\n", geomStartSector);
		useStartSector = geomStartSector;
	}

	if(optSpecifyStartSector) {
		printf("specified=%lu\n", optStartSector);
		useStartSector = optStartSector;
	}

	printf("filesystem=%lu\n", fsStartSector);
	if(useStartSector == fsStartSector) {
		printf("No changes are neccessary.\n");
		return 0;
	}

	if(!optWrite) return 0;
	if(lseek(device, 28L, SEEK_SET) < 0) {
		perror("lseek");
		return 2;
	}

	fsStartSector = fliplong(useStartSector);

	if(write(device, &fsStartSector, 4) != 4) {
		perror("write");
		return 2;
	}
	if(fsync(device)) {
		perror("fsync");
		return 2;
	}
	if(close(device)) {
		perror("close");
		return 2;
	}
	
	printf("Filesystem start sector altered to %lu\n", useStartSector);
	return 1;
}
