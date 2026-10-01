#ifndef HALO_VITA_EXCPT_H
#define HALO_VITA_EXCPT_H
/* Type declarations only. Vita exception reporting must be implemented;
 * do not silently turn exception handlers into success paths. */
typedef enum _EXCEPTION_DISPOSITION {
 ExceptionContinueExecution, ExceptionContinueSearch,
 ExceptionNestedException, ExceptionCollidedUnwind
} EXCEPTION_DISPOSITION;
#define EXCEPTION_EXECUTE_HANDLER 1
#define EXCEPTION_CONTINUE_SEARCH 0
#define EXCEPTION_CONTINUE_EXECUTION -1
#endif
