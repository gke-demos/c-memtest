#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h> // For time(NULL)

#define CGROUP_BASE_PATH "/sys/fs/cgroup"
#define CGROUP_PROCFS_PATH "/proc/self/cgroup"
#define CGROUP_MEMORY_MAX_FILENAME "memory.max"
#define CGROUP_PATH_MAX 512
#define BUFFER_SIZE 256

/**
Large portion of code generated with Gemini 2.5 flash

Write a simple application in C which gets the cgroupv2 memory limit,
allocates memory for 90% of that limit and then checks to see if the
cgroupv2 memory limit has changed every 1 second

Write a simple application in C which gets the cgroupv2 memory limit
allocates memory for 90% of that limit and then checks to see if the
cgroupv2 memory limit has changed every 1 second

 */

// Function to get the cgroupv2 path for the current process
int get_cgroup_path(char *cgroup_path_buffer, size_t buffer_size) {
    FILE *fp = NULL;
    char line[BUFFER_SIZE];
    char *cgroup_rel_path = NULL;
    int found = 0;

    fp = fopen(CGROUP_PROCFS_PATH, "r");
    if (fp == NULL) {
        fprintf(stderr, "Error opening %s: %s\n", CGROUP_PROCFS_PATH, strerror(errno));
        return -1;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        // cgroupv2 entries typically start with "0::"
        // Example: 0::/user.slice/user-1000.slice/session-2.scope
        if (strncmp(line, "0::", 3) == 0) {
            // Find the start of the relative path after "0::"
            cgroup_rel_path = line + 3;
            // Remove trailing newline
            cgroup_rel_path[strcspn(cgroup_rel_path, "\n")] = 0;
            found = 1;
            break;
        }
    }
    fclose(fp);

    if (!found) {
        fprintf(stderr, "Could not find cgroupv2 entry in %s. Ensure cgroupv2 is enabled and in use.\n", CGROUP_PROCFS_PATH);
        return -1;
    }

    // Construct the full path to memory.max
    // CGROUP_BASE_PATH + cgroup_rel_path + CGROUP_MEMORY_MAX_FILENAME
    int snprintf_ret = snprintf(cgroup_path_buffer, buffer_size, "%s%s/%s",
                                CGROUP_BASE_PATH, cgroup_rel_path, CGROUP_MEMORY_MAX_FILENAME);

    if (snprintf_ret >= buffer_size || snprintf_ret < 0) {
        fprintf(stderr, "Error constructing cgroup path or buffer too small.\n");
        return -1;
    }

    // Verify the path exists and is readable
    if (access(cgroup_path_buffer, R_OK) != 0) {
        fprintf(stderr, "Error: Cannot access cgroup memory.max file '%s': %s\n", cgroup_path_buffer, strerror(errno));
        fprintf(stderr, "Ensure your process has read permissions and the path is correct.\n");
        return -1;
    }

    return 0; // Success
}


// Function to read the memory limit from memory.max
long long get_memory_limit(const char *cgroup_path) {
    FILE *fp = NULL;
    char buffer[BUFFER_SIZE];
    long long limit = -1;

    fp = fopen(cgroup_path, "r");
    if (fp == NULL) {
        fprintf(stderr, "Error opening %s: %s\n", cgroup_path, strerror(errno));
        return -1;
    }

    if (fgets(buffer, sizeof(buffer), fp) != NULL) {
        // Remove trailing newline character if present
        buffer[strcspn(buffer, "\n")] = 0;

        if (strcmp(buffer, "max") == 0) {
            limit = -1; // Indicate no specific limit
        } else {
            limit = atoll(buffer);
        }
    } else {
        fprintf(stderr, "Error reading from %s: %s\n", cgroup_path, strerror(errno));
    }

    fclose(fp);
    return limit;
}

int main() {
    char cgroup_memory_max_path[CGROUP_PATH_MAX];
    long long initial_memory_limit_bytes;
    long long current_memory_limit_bytes;
    char *allocated_memory = NULL; // Change to char* for byte-level access
    size_t allocation_size;
    size_t new_allocation_size;

    setbuf(stdout, NULL); 

    printf("Starting cgroupv2 memory limit monitoring application.\n");

    // Get the cgroup path
    if (get_cgroup_path(cgroup_memory_max_path, sizeof(cgroup_memory_max_path)) != 0) {
        fprintf(stderr, "Failed to determine cgroupv2 memory.max path. Exiting.\n");
        return 1;
    }
    printf("Monitoring cgroupv2 memory limit at: %s\n", cgroup_memory_max_path);


    // Get initial memory limit
    initial_memory_limit_bytes = get_memory_limit(cgroup_memory_max_path);
    if (initial_memory_limit_bytes == -1) {
        printf("Initial memory limit: No specific limit ('max') or error reading.\n");
        printf("Cannot allocate 90%% of 'max'. Exiting.\n");
        return 1; // Cannot proceed without a concrete limit
    } else if (initial_memory_limit_bytes == 0) {
        printf("Initial memory limit is 0 bytes. Cannot allocate memory. Exiting.\n");
        return 1;
    } else {
        printf("Initial memory limit: %lld bytes (%.2f MB)\n",
               initial_memory_limit_bytes, (double)initial_memory_limit_bytes / (1024 * 1024));
    }

    // Allocate 90% of the initial limit
    allocation_size = (size_t)(initial_memory_limit_bytes * 0.90);
    if (allocation_size == 0) {
        fprintf(stderr, "Calculated allocation size is 0 bytes. Exiting.\n");
        return 1;
    }

    printf("Attempting to allocate %zu bytes (%.2f MB) for 90%% of the limit...\n",
           allocation_size, (double)allocation_size / (1024 * 1024));

    allocated_memory = (char *)malloc(allocation_size); // Cast to char*
    if (allocated_memory == NULL) {
        fprintf(stderr, "Failed to allocate memory: %s\n", strerror(errno));
        // It's possible to fail allocation even if the limit is set, due to system-wide memory pressure.
        return 1;
    }
    printf("Successfully allocated %zu bytes.\n", allocation_size);

    // Keep the allocated memory "hot" to ensure it's actually resident if possible
    // (though the kernel might still swap it out if under pressure).
    // Writing to it ensures the pages are touched.
    printf("Filling allocated memory with data...\n");
    for (size_t i = 0; i < allocation_size; ++i) {
        allocated_memory[i] = (char)(i % 256); // Fill with a simple pattern
    }
    printf("Memory filling complete.\n");

    // Monitor for changes every 1 second
    while (1) {
        sleep(1); // Wait for 1 second

        current_memory_limit_bytes = get_memory_limit(cgroup_memory_max_path);

        if (current_memory_limit_bytes == -1) {
            printf("[%ld] Current memory limit: No specific limit ('max') or error reading.\n", time(NULL));
            if (initial_memory_limit_bytes != -1) {
                 printf("WARNING: Limit changed from a specific value to 'max' or an error occurred while reading.\n");
            }
        } else if (current_memory_limit_bytes == initial_memory_limit_bytes) {
            printf("[%ld] Current memory limit: %lld bytes (%.2f MB) - No change.\n",
                   time(NULL), current_memory_limit_bytes, (double)current_memory_limit_bytes / (1024 * 1024));
        } else {
            printf("[%ld] Current memory limit: %lld bytes (%.2f MB) - CHANGED from %lld bytes (%.2f MB)!\n",
                   time(NULL), current_memory_limit_bytes, (double)current_memory_limit_bytes / (1024 * 1024),
                   initial_memory_limit_bytes, (double)initial_memory_limit_bytes / (1024 * 1024));
            // increase the size of allocated memory and fill it again
            new_allocation_size = (size_t)(current_memory_limit_bytes * 0.90);
            allocated_memory = (char *)realloc(allocated_memory, new_allocation_size);
            printf("Filling newly allocated memory with data...\n");
            for (size_t i = 0; i < new_allocation_size; ++i) {
                allocated_memory[i] = (char)(i % 256); // Fill with a simple pattern
            }
            printf("Memory filling complete.\n");
            // Update the initial values for future comparisons
            initial_memory_limit_bytes = current_memory_limit_bytes; 
            allocation_size = new_allocation_size;
            
        }
    }

    // This part is theoretically unreachable in this infinite loop,
    // but good practice for resource cleanup if the loop were to break.
    free(allocated_memory);
    return 0;
}