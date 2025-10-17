/**
 * Hello World Test Module
 * Simple module to test the dynamic loading system
 */

#include <stdint.h>

// Simple counter to show module is working
static int call_count = 0;

/**
 * Module initialization
 */
int module_init(void) {
    call_count = 0;
    // Return 0 for success
    return 0;
}

/**
 * Module cleanup
 */
void module_exit(void) {
    // Nothing to clean up
    call_count = 0;
}

/**
 * Example function that can be called
 */
int hello_get_count(void) {
    return call_count++;
}
