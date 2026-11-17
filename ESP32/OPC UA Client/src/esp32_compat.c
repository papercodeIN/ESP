/**
 * @file esp32_compat.c
 * @brief libc symbols that ESP-IDF lacks but the open62541 stack links
 *        against.
 *
 * - gethostname(): declared in ESP-IDF's sys/unistd.h but not implemented.
 *   The firmware sets the endpoint URL explicitly, so the value returned
 *   here is only cosmetic.
 * - signal(): the open62541 InterruptManager registers POSIX signals; none
 *   are used by this firmware, so a stub that always returns SIG_DFL is
 *   sufficient.
 */
#include <stddef.h>
#include <string.h>
#include <signal.h>

int gethostname(char *name, size_t len)
{
    static const char fallback[] = "esp32";
    if (name && len) {
        strncpy(name, fallback, len - 1);
        name[len - 1] = '\0';
    }
    return 0;
}

void (*signal(int signum, void (*handler)(int)))(int)
{
    (void)signum;
    (void)handler;
    return SIG_DFL;
}
