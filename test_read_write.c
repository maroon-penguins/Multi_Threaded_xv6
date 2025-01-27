#include "types.h"
#include "user.h"
#include "syscall.h"

int main() {
    char buffer[128];
    char readBuffer[128];

    // Initialize buffer with some data
    strcpy(buffer, "Hello, Resource!");

    // Request resource 0
    if (requestresource(0) < 0) {
        printf(1, "Failed to request resource 0\n");
        exit();
    }

    // 1. Read from resource 0 (should be empty or contain default data)
    if (readresource(0, 0, sizeof(readBuffer), readBuffer) < 0) {
        printf(1, "Failed to read from resource 0\n");
        exit();
    }
    printf(1, "Initial data read from resource 0: %s\n", readBuffer);

    // 2. Write data to resource 0
    if (writeresource(0, buffer, 0, strlen(buffer) + 1) < 0) {
        printf(1, "Failed to write to resource 0\n");
        exit();
    }
    printf(1, "Data written to resource 0: %s\n", buffer);

    // 3. Read data from resource 0 again to verify the write
    if (readresource(0, 0, sizeof(readBuffer), readBuffer) < 0) {
        printf(1, "Failed to read from resource 0 after write\n");
        exit();
    }
    printf(1, "Data read from resource 0 after write: %s\n", readBuffer);

    // 4. Write new data to resource 0
    strcpy(buffer, "New data for resource!");
    if (writeresource(0, buffer, 0, strlen(buffer) + 1) < 0) {
        printf(1, "Failed to write new data to resource 0\n");
        exit();
    }
    printf(1, "New data written to resource 0: %s\n", buffer);

    // 5. Read data from resource 0 again to verify the new write
    if (readresource(0, 0, sizeof(readBuffer), readBuffer) < 0) {
        printf(1, "Failed to read from resource 0 after new write\n");
        exit();
    }
    printf(1, "Data read from resource 0 after new write: %s\n", readBuffer);

    // Release resource 0
    if (releaseresource(0) < 0) {
        printf(1, "Failed to release resource 0\n");
        exit();
    }

    printf(1, "Resource 0 released successfully\n");

    exit();
}