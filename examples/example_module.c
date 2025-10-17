/**
 * Example Loadable Module for Chromatic MCU
 * 
 * This demonstrates how to create a simple loadable module.
 * Compile with: tools/build_module.py examples/example_module.c -o example.mod -n "Example" -d "Example module"
 */

#include <stdint.h>

// Module must implement these two functions

/**
 * Module initialization - called when module is loaded
 * @return 0 on success, non-zero on error
 */
int module_init(void) {
    // Initialize your module here
    // Note: Be careful with external dependencies
    // Only use functions that are guaranteed to be available
    
    return 0;  // Success
}

/**
 * Module cleanup - called when module is unloaded
 */
void module_exit(void) {
    // Clean up your module here
    // Free any allocated resources
}

/**
 * Example module function
 */
void example_function(void) {
    // Your module code here
}
