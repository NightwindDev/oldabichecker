/**
 * Copyright (c) 2024 Nightwind. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <mach/mach.h>
#include <mach/machine.h>
#include <mach-o/fat.h>
#include <mach-o/loader.h>

int main(int argc, char *argv[], char *envp[]) {
	// Check args
	if (argc != 2 && argc != 3) {
		fprintf(stderr, "\nExpected: oldabichecker <path/to/executable>\n\tAdd -v to the end for verbose logging\n\n");
		return EXIT_FAILURE;
	}

	// Get path to executable
	const char *path_to_exec = argv[1];
	// Verbose logging, false by default
	bool verbose = false;

	// If the user passes in -v as the third argument, set verbose logging to true
	if (argc == 3 && !strcmp(argv[2], "-v")) {
		verbose = true;
	}

	// Getting the file descriptor for the file in readonly mode
	int fd = open(path_to_exec, O_RDONLY);

	// If the file wasn't correctly opened, throw error and clean up
	if (fd == -1) {
		fprintf(stderr, "Failed to open executable at path \"%s\", error: %s\n", path_to_exec, strerror(errno));
		close(fd);
		return EXIT_FAILURE;
	}

	// Get the file size
	const off_t file_size = lseek(fd, 0, SEEK_END);
	// Come back to beginning of file
	lseek(fd, 0, SEEK_SET);

	// Map the Mach-O in memory
	void *map = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, fd, 0);

	// If the mapping failed, throw error and clean up
	if (map == MAP_FAILED) {
		fprintf(stderr, "Failed to map file!\n");
		perror("mmap");
		munmap(map, file_size);
		close(fd);
		return EXIT_FAILURE;
	}

	// Interpret the map as a 64 bit mach header
	const struct mach_header_64 *mh = (const struct mach_header_64 *)map;

	// If the Mach-O is a FAT type, then more work needs to be done
	if (mh->magic == FAT_CIGAM || mh->magic == FAT_MAGIC) {
		if (verbose) fprintf(stdout, "FAT file, finding arm64e slice...\n");

		// Interpret the map as a fat_header
		const struct fat_header *fh = (const struct fat_header *)map;
		// Get the first arch from the fat_header
		struct fat_arch *arch = (struct fat_arch *)(map + sizeof(struct fat_header));

		// Loop over the arches in the fat_header
		for (uint32_t i = 0; i < OSSwapBigToHostInt32(fh->nfat_arch); ++i) {
			// Interpret each arch as a slice header
			const struct mach_header_64 *sh = (const struct mach_header_64 *)(map + OSSwapBigToHostInt32(arch->offset));

			// Get the cpu subtype without any feature flags, if arm64e, check for the new ptrauth ABI
			if ((sh->cpusubtype & ~CPU_SUBTYPE_MASK) == CPU_SUBTYPE_ARM64E) {
				if (sh->cpusubtype & CPU_SUBTYPE_PTRAUTH_ABI) {
					fprintf(stdout, "Found new ABI.\n");
					goto cleanup;
				} else {
					fprintf(stdout, "Found old ABI.\n");
					goto cleanup;
				}
			}

			// Move on to the next arch
			arch += 1;
		}

		// If there was no arm64e slice found, then inform user
		fprintf(stdout, "No arm64e slice found!\n");
	} else if (mh->magic == MH_MAGIC_64 || mh->magic == MH_CIGAM_64) {
		// If the program is dealing with a thinned (non FAT) Mach-O, run the operations directly

		if (verbose) fprintf(stdout, "Thinned file, running directly\n");

		// Get the cpu subtype without any feature flags, if arm64e, check for the new ptrauth ABI
		if ((mh->cpusubtype & ~CPU_SUBTYPE_MASK) == CPU_SUBTYPE_ARM64E) {
			if (mh->cpusubtype & CPU_SUBTYPE_PTRAUTH_ABI) {
				fprintf(stdout, "Found new ABI.\n");
			} else {
				fprintf(stdout, "Found old ABI.\n");
			}
		} else {
			fprintf(stdout, "No arm64e slice found!\n");
		}
	} else {
		fprintf(stderr, "File is not an executable! Make sure you're running the program on an executable file and not a .deb file.\n");
	}

	// Clean up after ourselves
cleanup:
	munmap(map, file_size);
	close(fd);

	return EXIT_SUCCESS;
}