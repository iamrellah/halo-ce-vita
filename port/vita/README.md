# Native-source Vita prototype

Private prototype pinned to upstream3012128fa5d9882cd9d36038f9aceae741bde78e.
This is an engine-source port investigation, not the existing Xita x86 lifter.
No Vita executable or performance improvement is established yet.

## Bring-up sequence

1. Add ARM32/Vita compile configuration. Preserve32-bit pointers,16-bit wide
   characters, and serialized structure layouts; audit MSVC aggregate returns,
   alignment, inline assembly and floating-point assumptions. Do not disguise
   ARM as x86 simply to bypass architecture checks.
2. Compile engine units with Vita-compatible headers and record unresolved
   dependencies. Compilation is allowed locally; runtime tests only on Vita.
3. Implement startup, memory, file I/O and synchronization on Vita. Verify a
   minimal executable on hardware before claiming the engine boots.
4. Adapt Direct3D8-facing calls to Xita's GXM renderer. Audit pointer ownership,
   original Xbox shader formats, resource lifetime and render ordering; the
   existing translated-call boundary is not automatically a native ABI.
5. Connect audio/input and load compatible2342 game data. Existing Xita map
   data was identified as2276; do not bypass format/version checks.
6. Reach a rendered menu and playable b30, then compare total frame times,
   slow frames, NPC combat and effects against the current Xita build.

## Acceptance

The user's sustained20FPS hardware goal and gameplay/stability requirements
remain unchanged. Native compilation alone is not performance evidence.
Retain the working Xita artifact and saves. No public push, asset redistribution,
Pi use or emulator execution. Preserve upstream notices and dependency licenses.

## Current compilation evidence

`python3 port/vita/compile_engine.py source/math/*.c
source/models/model_animations.c source/objects/objects.c` successfully emits
11 ARM32 objects. Timestamped receipts and per-unit compiler logs are under
build/vita. This is compilation only: unresolved platform symbols, aggregate
ABI, serialized layouts and hardware behavior remain to be verified. The
native matrix C path is enabled for HALO_VITA; no x86 inline assembly executes.
Atomic functions have Vita-specific external names, not no-op implementations.

`--engine` selects the upstream halobetacache project's467 C entries (excluding
Missing entries and the C++ intimacy source). It is an engine compilation census,
not a complete platform build. The first pass compiled383 entries. The second
pass uses upstream's generated MSVC tag/weak-inline compatibility declarations
and the xbox define, with DEBUG omitted intentionally for a release bring-up.
Undefined weak symbols must be rejected at link time. Keep incompatible function
pointer diagnostics: aggregate parameter/return mismatches need ARM ABI fixes.

## UTF-16 and ABI follow-up

Targeted builds now also compile hs_runtime.c, terminal.c, cseries.c and
unicode.c. The shared upstream msvc_wide.c compiles for ARM32 with Vita headers.
Vita wide headers route text operations to these 16-bit implementations; file
wrappers intentionally require halo_vita_fopen/freopen/remove, still pending.
The tokenizer uses thread-local storage and needs Vita link/runtime verification.

The compiler recognized a wide string loop and emitted a CRT wcslen call,
which would bypass the compatibility implementation. The prototype now uses
-fno-builtin to prevent that substitution. The wide runtime object no longer
references CRT wcslen. A fresh full census is required with this flag; previous
counts do not establish a complete build with the updated configuration.
No executable, hardware validation, or FPS improvement is established.

Full census 4 compiled 448/467 entries with the UTF-16 headers and -fno-builtin.
Bitmap/decal color packing uses the existing C conversion path on Vita, with
unsigned packing shifts. Bink allocation failure uses a trap, preserving fatal
behavior. AI optional-prop sorting uses the int-returning qsort callback ABI.
Remaining failures are retained in engine-build-4-summary.json, including the
missing linker_common.c entry. No missing subsystem was stubbed out.

## Callback and platform follow-up

Five further targeted units compile: vertex shader runtime, transparent
geometry, object lights, bitmap groups, and recorded animation definitions.
API outputs now use UINT; callback adapters call the original typed functions
without incompatible function-pointer casts. This is not a fresh full census.

port/vita/src/atomics.c implements the five interlocked operations with
sequentially consistent ARM atomics. Compile-time checks require 32-bit,
lock-free words. Disassembly confirms LDREX/STREX retry loops and DMB barriers;
the object has no undefined runtime helpers. Hardware concurrency validation
and a complete platform link remain outstanding.

Full census 5: 458/467 C entries compile. Vita TIFF uses fstat and binary
file descriptor semantics; debug drawing preserves the recovered callee's
single-argument behavior; device persistence receives a device pointer.
AI burst-target point/vector addition uses typed components in the original
operand order, avoiding incompatible pointer reinterpretation.

Link prerequisite: upstream port/linux/src/halo_linker_common.c describes
weak storage whose sizes are upper bounds derived from symbol spacing, plus
missing reconstructed functions. Audit this storage against real owner types
before reusing it for Vita; a clean compile is not evidence of complete engine
reconstruction. The missing source/linker_common.c is still reported explicitly.

Vehicle, AI debug and UI event-handler units additionally compile in targeted
builds. Vehicle animation now passes the actual two-byte update structure;
point/vector operations preserve the original component order. AI debug's
92-byte path output region now uses path_result, with a size assertion. UI
profiles use game_variant rather than a raw byte placeholder; network address
and key fields are copied into typed aligned locals before synchronous use.
No hardware behavioral validation has occurred for these changes.

Vita profiling now queries sceKernelGetSystemTimeWide through profile_clock.c
and uses a 1 MHz frequency, replacing the Xbox RDTSC assumption. Profiling
GPU-stall producers must use that same unit when the graphics bridge is added.
This clock path compiles but has not been exercised on hardware.

The partial engine symbol audit reports 462 objects and 599 symbols unresolved
within those objects, including 108 D3D-prefixed names. This deliberately
excludes platform objects and missing engine units; it is not an executable
link result. See build/vita/partial-symbol-audit.json for per-symbol callers.

Do not cast away the editor scripted-camera error: flying_camera_action and
camera_control have different fields, and scripted_camera_command extends
camera_command. Trace consumers and output sizes before adapting that call.

Camera boundary audit: director.c allocates a 104-byte observer_command and
populates camera input seconds_elapsed at offset 4. The flying-camera header
had described those bytes as padding, and camera_command omitted the trailing
parameter flags and timers. The common declarations now expose those fields;
scripted_camera_update uses them directly. Size and critical output offsets
are asserted. All five targeted camera units compile; full census 6 follows
this header change and reports 463/467 units compiled. Hardware camera behavior
is not yet verified. Remaining entries: Xbox shell, HUD return-address helper,
Windows stack walking and the absent linker_common.c.

HUD diagnostics use a caller-site ARM return-address builtin instead of the
x86 EBP helper. The sentinel checks remain; the builtin is diagnostic only,
not a stack-protector replacement. HUD draw/weapon/nav-point units compile.

A partial relocatable link now combines 464 engine and 3 platform objects
without duplicate-symbol errors. It allows unresolved references and is NOT
a Vita executable. Its 543 undefined symbols include 108 graphics, 41 audio
and 17 network names by prefix; the other 377 include CRT helpers, platform
services and missing engine definitions. Receipts and full symbol list are
in build/vita/partial-link*. No unresolved symbols were silently supplied.

## Native prototype file routing

files.c maps d: and relative paths to ux0:data/xita-native/2342/data. Other
letter drives map beneath ux0:data/xita-native/2342/saves/<drive>. Both stdio
and the UTF-16 wrappers share this mapper. Game-data writes/removals are
rejected; traversal and oversized paths fail with errno. No files or
directories are created by compilation. Parent-directory creation and Win32
handle APIs remain pending, and this is not yet complete filesystem support.
freopen(NULL, ...) is explicitly unsupported until stream ownership can
preserve the read-only data boundary. No existing Xita paths are reused.

The two file-layer units compile and ARM-target static analysis reports no
diagnostics for files.c. This does not replace physical Vita file-I/O tests.

file_roots.c now provides startup-time creation of the isolated data/save
roots and drive directories. Existing paths are checked to be directories;
Vita API failures are preserved, validation failures use -1 with errno.
It compiles and passes static analysis, but is not called by a linked startup
yet and has not run on the Vita. This does not yet create nested save folders
or implement CreateFile/ReadFile and other Xbox handle APIs.

File-handle audit: cache_files_windows.c uses OVERLAPPED.hEvent as a pointer
to a completion flag in its ReadFileEx path, and pumps callbacks with
SleepEx(0, TRUE). Preserve issuing-thread callback dispatch; do not treat
that field as an event handle for Ex requests or invoke callbacks inline.

last_error.c implements GetLastError/SetLastError using pthread keys with
once-only initialization, plus errno conversion for the coming file APIs.
It compiles; the installed Vita pthread archive defines all four required
TLS functions. Hardware thread-isolation validation is still pending.

handles.c provides 256 public handle slots with generation-tagged IDs and
retained resource lifetimes. Acquire/release protects operations against
concurrent CloseHandle; close detaches the token, with destruction only after
the final retained operation releases. Destructors run outside the table lock.
Generation exhaustion retires the slot rather than wrapping into a stale ID.
Exhaustion returns an explicit error. File/event implementations must retain
objects throughout operations; this does not itself implement waiting or I/O.
The unit compiles and static analysis is clean. Hardware concurrency stress
tests remain required before considering the lifetime contract verified.

CreateFileA now opens native descriptors through the isolated path mapper,
tracks read/write sharing between native file handles, handles the five
creation dispositions, rejects directory opens, and publishes retained
handles with a descriptor-closing destructor. Each file has an I/O mutex for
subsequent positioned and sequential operations. It compiles and static
analysis is clean. It has not run on hardware.

Still required: ReadFile/WriteFile, completion queues, seeks/size/flush, and
sharing enforcement for deletion and stdio interoperability. Overlapped and
no-buffering flags are recorded at open but their operation semantics are
not implemented yet. Prototype read-only data rules reject potentially
creating opens, including OPEN_ALWAYS, on the data drive. Unsupported
attributes/security/template parameters return explicit errors.

GetFileSize and FlushFileBuffers now retain handles across native calls and
serialize with the file I/O mutex. Failures preserve errno before releasing
the final reference, which may destroy the descriptor. Flush requires write
access and retries interrupted calls. SDK libc disassembly confirms fsync
uses sceIoSyncByFd for ordinary file descriptors; it is not a no-op shim.
The unit compiles and passes static analysis; hardware save durability and
file-size behavior remain unverified. No game or user saves were accessed.

file_transfer.c supplies the shared positioned/sequential read/write core.
Callers must retain a FILE-typed handle throughout the operation. It checks
access, bounds offsets to the actual off_t range, serializes descriptor access,
retries EINTR, handles short transfers/EOF, and returns actual bytes and error.
It compiles and static analysis passes; libc provides pread/pwrite. Public
ReadFile/WriteFile and Ex completion dispatch still need wiring and hardware
validation, including no-buffering alignment and GPU resource invalidation.
No performance or loading-speed claim follows from this compile result.

completion.c adds a retained issuing-thread queue using pthread keys.
Requests embed their completion node before I/O submission, so posting after
I/O requires no allocation. Dispatch runs callbacks only on the current
issuer, outside the queue lock, with a 64-callback batch limit. Thread exit
closes its queue and disposes queued request storage without executing game
callbacks. Retained worker references keep the queue alive through late posts.
Cleanup handlers cover callback-triggered thread exit. Each node may be
posted once; run must not dispose its own request. The dispatcher owns that
cleanup. Compilation/static analysis pass; concurrency behavior remains
unverified on Vita. SleepEx/waits and the actual I/O worker still need wiring.

Alertable waiting: completion queues now use CLOCK_MONOTONIC condition
variables; the installed pthread implementation accepts that clock. Waits
check the queue predicate under its lock and release retained queues/locks
on thread cancellation. SleepEx pumps issuing-thread callbacks and reports
WAIT_IO_COMPLETION. Non-alertable Sleep yields at zero or sleeps without
dispatching. Both units compile and pass static analysis. Hardware tests for
wakeup races, timeout accuracy, reentrancy and cancellation remain pending.

ReadFileEx/WriteFileEx now submit to one bounded 64-request pthread worker.
Accepted requests retain file handles and issuer queues, transfer at captured
offsets, publish status, and post preallocated completion nodes. hEvent is
not interpreted as an event in the Ex path. Completed callbacks count toward
the capacity until consumed/disposed. Shutdown drains transfers and joins the
worker; issuers must retire callbacks before their buffers are freed.

This unit compiles and static analysis is clean, but it is NOT deployment
qualified: cancellation on issuer exit, duplicate OVERLAPPED reuse, no-buffering
alignment, worker shutdown integration and physical hardware concurrency tests
remain required. Caller buffers and OVERLAPPED storage must outlive requests.
The build does not yet supply ordinary ReadFile/WriteFile or full event waits.

Async request reuse: accepted OVERLAPPED addresses remain in an active list
until callback entry or request disposal, including completed requests whose
callbacks have not run yet. Duplicate submissions fail before touching the
original record. Callback entry unregisters first so reentrant submission of
the next operation is permitted; old-request disposal cannot unregister the
new request. Compile/static-analysis checks pass; hardware stress tests remain.

CancelIo now marks unfinished requests for the calling issuer/file. Queued
requests skip transfers; already-running transfers drain before publishing
ERROR_OPERATION_ABORTED. Cancellation does not roll back writes. Completion
queue teardown stops accepting posts, cancels/drains issuer transfers, then
disposes queued callbacks. Work-done publication occurs after the final
OVERLAPPED write under the worker mutex, preventing the drain from returning
while the worker still writes that record. Lock order never nests a queue
lock around the drain. Compile/static checks pass; runtime tests still needed.

This protects thread teardown only when storage remains valid through the
drain. Thread wrappers and caller cleanup must drain BEFORE freeing buffers;
a TLS destructor cannot undo earlier caller cleanup or forced termination.
This lifecycle integration remains a deployment gate.

Ordinary file I/O: ReadFile/WriteFile now support synchronous, sequential
transfers on non-overlapped file handles. They retain handles through transfer,
return actual byte counts (including partial transfers), preserve errors across
cleanup, and treat EOF as a successful short read. SetFilePointer serializes
native seeks with transfers and rejects unrepresentable SDK offsets instead of
truncating them. These paths are used by tag files, cache headers and saves.
Both new units compile for ARM Vita and pass Clang static analysis with zero
diagnostics; receipt: build/vita/file-sync-position-analysis.json.

This is not full Win32 I/O completion support: ordinary calls with OVERLAPPED
return ERROR_CALL_NOT_IMPLEMENTED without modifying the record; asynchronous
handles without a record return ERROR_INVALID_PARAMETER. Event completion,
GetOverlappedResult, no-buffering alignment and physical runtime validation
remain outstanding. No prototype was deployed and no FPS improvement is claimed.

Event primitives: anonymous CreateEventA, SetEvent and ResetEvent now use
retained EVENT handles. Manual-reset signals broadcast; auto-reset signals
wake a waiter and a successful wait consumes the signal. The internal wait
uses a monotonic absolute deadline, rechecks the predicate after spurious
wakeups/timeouts, and unlocks on pthread cancellation. Its caller must retain
and release the handle, including cancellation cleanup. Named events and
security attributes fail explicitly until their semantics are implemented.

The event unit cross-compiles and passes static analysis without diagnostics
(build/vita/events-analysis.json). This does not validate scheduling behavior.
Public multi-type/alertable waits and ordinary overlapped file completion are
not wired yet. Hardware validation must exercise signal-before-wait, timeouts,
manual broadcast/reset, auto-reset consumption, close during retained waits,
and cancellation before deploying an engine build. No new runtime/FPS result.

Ordinary overlapped completion is now connected: ReadFile/WriteFile submit
OVERLAPPED requests on asynchronous handles to the same bounded worker used
by Ex calls, returning FALSE/ERROR_IO_PENDING when accepted. A non-null hEvent
is validated and retained before acceptance, reset while holding the request
registry lock, and signaled after byte-count/status publication. Ex callbacks
continue to ignore hEvent. Both paths share duplicate-record rejection.
Ordinary completions retire without requiring alertable callback dispatch.

GetOverlappedResult polls status or waits on the worker completion condition,
including requests without events. It validates active request/file identity,
uses acquire publication, and releases the lock/file reference on cancellation.
Completed caller-owned records must remain valid and cannot be concurrently
reused while being queried. Both edited units compile and pass static analysis
(build/vita/overlapped-events-analysis.json). No runtime validation performed.

Outstanding: public event/multi-object/alertable waits; positioned ordinary
calls on synchronous handles (currently rejected); no-buffering alignment;
thread exit/drain integration; platform test executable and physical Vita
concurrency tests. These supersede the earlier ordinary-overlapped unsupported
note, but do not establish a bootable engine or any FPS benefit.

Standalone platform test: `python3 port/vita/build_platform_test.py` compiles
all thirteen I/O/event support units plus platform_test.c and links a Vita ELF
with --no-undefined. It packages a separate test VPK but does NOT execute or deploy it. The test uses
only the isolated prototype namespace, writes platform-test.txt with START and
PASS/FAIL, and covers sequential write/read/EOF/seek, stale event handles,
retained event lifetime, manual/auto reset, asynchronous event completion, and
issuing-thread Ex callbacks. It is a smoke test, not concurrency qualification.
No tests have run on hardware yet; the engine itself still does not link.

Link warnings are preserved in build/vita/platform-test-link.log: SDK startup
objects use different wchar/enum attributes from the engine ABI. This harness
does not intentionally pass wide characters or enum objects to the CRT, but
ABI boundary review and SELF packaging/import verification remain required
before deployment. No warning has been suppressed or treated as runtime proof.

The platform test now packages as build/vita/xita-platform-test.vpk with its
own title ID XITATST01. No unresolved symbols (including weak ones) remain in
this test ELF. vita-elf-create accepts its imports/relocations; a safe SELF
is generated. Packaging receipts/logs are in build/vita/platform-package.json
and platform-package-*.log. SDK wchar/enum linker warnings remain recorded;
no wide-character or enum-valued platform boundary is intentionally exercised
by this smoke test. This is not a complete engine ABI qualification.

Install/launch must use the separate test application. The existing Xita
updater accepts game runtimes and is not an arbitrary executable launcher;
do not disguise this test as a CE/Halo2 runtime or replace the installed game.
The test has not been deployed or run. Its output, when run, is
ux0:data/xita-native/2342/platform-test.txt. START alone is not a pass.

Fresh engine census and link audit: 464/467 engine units compile, with the
same shell_xbox.c, stack_walk_windows.c and missing linker_common.c failures.
`python3 port/vita/audit_link.py <full-engine-compile-receipt>` recompiles the
platform units and builds a relocatable aggregate using only current successful
engine objects plus explicit platform sources. The test main and extracted SDK
diagnostic objects are excluded. Latest aggregate: 480 objects, 562 undefined
symbols, including 15 weak references. This count includes libc/SDK dependencies
and is not a count of functions requiring new implementations; adding platform
code adds such references, so comparison with the old 543 is not a regression
measure. This audit does not produce an executable.

The unresolved weak set includes normalize3d, distance3d, vector_from_points3d,
color packing, random helpers, collision_test_line and spline math. Resolving
inline-definition ownership is the next correctness priority: accepting these
as null at final link would hide missing math/collision code. Full list and
commands: build/vita/partial-link-unresolved.txt and partial-link-receipt.json.

Inline ownership fix: the upstream port/linux/game/msvc_comdat.c already emits
external pick-any definitions from the original math/bitmap/collision headers.
It now participates in the Vita aggregate, with HALO_VITA and the same compiler
flags as the engine. All 15 previously unresolved weak helpers have nonempty
code definitions (build/vita/comdat-definitions.json). No replacement math or
placeholder functions were introduced. The aggregate now contains 481 objects,
547 undefined symbols (including SDK/CRT), and ZERO unresolved weak references.
audit_link.py exits with failure if unresolved weak references recur. This
closes a link correctness hole, not the remaining engine/startup/GXM work; no
hardware performance claim follows from it.

Performance clocks: QueryPerformanceCounter returns the same Vita system
microsecond clock as engine profiling, QueryPerformanceFrequency returns the
shared HALO_VITA_CLOCK_FREQUENCY (1,000,000), and GetTickCount converts to
milliseconds before truncation. Null counter outputs fail explicitly. The
engine profiler now includes the shared clock header/constant. Compilation
and kernel-clock static analysis pass; the platform-test VPK links/packages
with clock checks around a 20 ms Sleep. Those checks have NOT run on Vita.
Rasterizer callbacks calling QueryPerformanceCounter inherit these units;
future GXM stall timestamps must also use this clock. Latest partial-link
counts predate kernel_clock.c and must be refreshed before final engine link.

Public event waits now support alertable callbacks without periodic polling.
Event signals and completion posts notify a shared generation/condition hub;
waiters sample before checking predicates and sleep only if the generation has
not changed. Object/queue locks are never acquired under the hub lock, callbacks
run outside it, and cancellation cleanup releases the hub and retained event.
Absolute monotonic deadlines are checked even under continual notifications.
This currently supports EVENT handles only: mutex/thread/file handle waits
remain explicit failures until their predicates and ownership are implemented.
The test VPK now exercises ordinary and alertable public event waits; all units
compile/link and wait.c static analysis is clean. Hardware execution is pending.

Platform test v2 adds 1,000 bidirectional handshakes between two pthreads,
using public auto-reset event waits with five-second failure timeouts. Shared
payload reads/writes deliberately rely on event synchronization; each side
checks the expected sequence value, and independent last-error sentinels check
TLS isolation. Test accounting is atomic so it adds no counter data race. The
test source passes static analysis and the VPK rebuild succeeds, but these
checks have NOT executed. Scheduling onto distinct cores is not forced; this
is cross-thread stress, not proof of core affinity or full concurrency safety.

Recursive mutexes: CreateMutexA/ReleaseMutex and wait acquisition now track
ownership under a mutex, retain objects for the lifetime of ownership, and
maintain a per-thread owned list. Pthread TLS teardown abandons remaining owned
mutexes and notifies waiters; the next acquisition returns WAIT_ABANDONED while
acquiring ownership. Non-owner releases fail, recursion is counted, and named
objects/security attributes remain explicitly unsupported. Generic handle
acquisition is type-checked before wait dispatch. Thread/file waits are still
missing; forced termination bypassing pthread teardown is not supported.

Test v3 adds recursive ownership, competing-thread polling, wrong-owner release,
and owner exit/abandoned reacquisition. Rebuilt test VPK and static analysis of
mutex/wait/handle units pass. Nothing has run on hardware; this is not yet
concurrency or gameplay qualification. Audit receipts: mutex-wait-analysis.json.

Thread lifecycle: CreateThread uses detached Vita pthreads with at least 64 KiB
stack, rejects unsupported creation flags/security, and retains the thread
object until cleanup. CREATE_SUSPENDED gates the start routine; ResumeThread
releases it. GetExitCodeThread and public waits report completion only after
explicit issuer I/O drain and mutex abandonment. Cleanup clears TLS slots so
later pthread TLS destructors do not repeat it. Thread IDs are local monotonic
identifiers, not native kernel IDs; priority mapping is still missing.

Test v4 adds suspended startup, no premature execution, ResumeThread, completion
waits and exit-code retention. Build/package and lifecycle static analysis pass;
no hardware test has run. Stack-local I/O buffers must finish before their start
routine returns: post-return cleanup cannot repair expired caller storage.
Forced thread termination and arbitrary suspend counts remain unsupported.
A thread closed while still suspended remains suspended until process exit,
matching close-not-terminate behavior but requiring caller lifecycle discipline.

Thread priority mapping: installed libpthread disassembly shows priority range
128..191, default 160, and native priority = 319 - pthread priority. Its OSAL
setter discards sceKernelChangeThreadPriority errors. The Vita bridge therefore
calls the kernel directly and checks its result. Xbox ordinary hints -2..2 map
to native 175,167,159,151,143; idle/time-critical map to 191/128. These are
bring-up policy choices, not performance tuning results. Unsupported priorities
fail explicitly. Current-thread pseudo handle -2 uses the actual kernel ID.

Created threads publish a kernel ID before CreateThread returns, including
suspended start routines. IDs now supersede the earlier synthetic identifiers.
Priority changes serialize with completion publication. Test VPK adds priority
changes before ResumeThread and invalid-priority rejection. Compile/link/static
checks pass; hardware and scheduling behavior remain unverified. Direct changes
do not update pthread's cached priority; do not mix pthread priority queries
with the native bridge as a source of truth.

Native link/platform follow-up: refreshed partial link has 487 objects, 545
unresolved symbols and no unresolved weak symbols. This includes libc/Vita SDK
imports and is not an executable-link result. Added real CreateDirectoryA and
RemoveDirectoryA using translated isolated paths, mkdir/rmdir and Windows error
mapping; D: remains read-only. Security attributes explicitly unsupported.
Local platform test v6 adds creation, existing-directory, nonempty-removal,
missing-parent and read-only-drive checks. Compile/link/package pass; no Vita
execution claimed. Staged v5 remains unchanged; v6 only exists locally.

Native startup compilation: shell_xbox.c now has a HALO_VITA path that initializes
the isolated filesystem, checks the actual boolean graphics-preinitialize result,
and returns failure if shell_initialize fails. Xbox debugger module walking,
section protection, launch metadata and Windows SEH are excluded only on Vita;
they are not replaced with fake-success APIs. ARM compile succeeds and nm confirms
no DmWalk/XGetLaunchInfo/SEH imports from this unit. Native crash handling is still
platform-default. This is not a runnable game entry: physical_memory_allocate
still assumes fixed Xbox addresses and the graphics bridge is unresolved. Failed
partial initialization cleanup remains to be audited. The old complete-engine
receipt still lists shell_xbox as failed; only this new unit receipt proves its
compile success until the next full census. No deployment/runtime test performed.

Fresh full census (build/vita/compile-receipt-20260928T172743.022242Z.json):
465/467 engine units compile; stack_walk_windows.c and missing linker_common.c
remain. Partial link: 490 objects, 544 unresolved names, no unresolved weak names.
This includes SDK/libc references and does not represent 544 unimplemented engine
functions or a runnable executable.

Renderer reuse boundary: existing Xita xv_d3d/xv_ui_gxm record guest-address
resources and depend on xd3d state and scheduler/pump ownership. They cannot be
called unchanged with native engine pointers. The xv_texture_decode.h conversion
code instead accepts ordinary source/destination buffers. A new compile-only
probe (probe_xita_texture.py --xita-source ...) builds it for Vita ARM independently
of guest runtime, recording header hashes and unresolved symbols in
build/vita/texture-reuse/receipt.json. No source was copied into this project and
no decoder executed. Reuse must preserve Xita's license; native bitmap format IDs
must be mapped to Xbox D3D format IDs, with payload/output bounds checked before
calling. This is the next graphics adapter boundary to implement.

Native texture adapter: port/vita/graphics/texture_adapter.c maps engine bitmap
format IDs to the existing decoder's Xbox D3D format IDs, accepts one resolved
2D mip/face/slice, validates source/output spans and integer arithmetic, rejects
aliasing output and requires a real 256-entry palette for P8. Non-power-of-two
swizzled inputs, unsupported format holes and unsupported linear compressed/P8
layouts fail explicitly. Output is CPU ABGR8888; this does not map/upload a GXM
texture, resolve mip/face offsets, publish CPU caches or enable game rendering.
The probe now compiles this actual adapter against an explicit Xita checkout and
records its hash alongside dependency headers. ARM compilation and Clang static
analysis pass; pixel output and error-path runtime tests still need Vita execution.

Platform test v9 now links the real bounded texture adapter and existing Xita
texture decoder. Build requires explicit --xita-source PATH, forwarded to the
compile probe; dependency hashes remain recorded. Known-pixel cases include DXT1
opaque red, DXT1 transparent index, P8 palette channel mapping and linear ARGB
channel mapping. Rejection cases cover missing palettes, short source/output and
source/output aliasing; output canaries check writes. These tests compile/link
but have not executed on hardware. v9 is local only; staged v8 remains unchanged.

Native texture selection: texture_layout.c resolves 2D mips and engine-order cube
faces from bounded Xbox hardware payloads, accounting for compressed mip limits,
64-byte linear row pitch, 128-byte face alignment and face order {0,2,1,3,4,5}.
It validates complete resource size/address arithmetic and preserves output on
failure. Volume textures explicitly remain unsupported (3D Morton layout needs
a separate decoder), not treated as 2D slices. Platform test v10 adds known offset
cases for the rectangular DXT mismatch, cube faces, linear row pitch, truncated
payload and unsupported volume. Build/link pass; hardware execution pending.
v10 local only, staged v8 unchanged. No game rendering enabled.

Native texture upload implementation: texture_upload.c decodes one selected 2D
image into temporary CPU storage, copies rows into a 64-byte-stride uncached Vita
allocation, drains writes with DSB, maps READ for GXM and initializes ABGR8888
linear-strided texture state. Per-resource pins prevent destruction until caller
retires all published references; no automatic fence tracking is implemented.
The single render-owner contract requires retirement only after actual GPU
completion. Failed unmap/free retains ownership for retry. This bring-up path
uses transient RGBA decode storage; it is not a compressed zero-copy/performance
optimization or a complete mipmapped/cube texture bridge.

Local platform test v11 adds descriptor dimensions/data, known uploaded pixel,
pin/destruction refusal and release/free checks while GXM is initialized. No draw
or sampling occurs. Build/link pass; runtime and GPU visibility remain unverified.
Staged v8 unchanged, v11 local only, installed Halo untouched.

Dependency ownership audit: audit_link.py now accepts required --xita-source and
includes the actual texture adapter object. Refreshed partial link: 493 objects,
545 unresolved symbols, zero unresolved weak symbols. link_dependencies.py maps
each symbol to object users and checks exports in the selected Vita SDK/libc/math/
pthread archives: 158 have library providers, 110 are Xbox graphics names, 41
Xbox audio names, 11 Bink names and 225 need individual ownership review. Evidence
is build/vita/link-dependencies.json. These categories are static dependency
classification, not performance measurements; MSVC inline emission can make a
symbol appear in many objects without representing many executed call sites.
Archive exports also do not prove a complete final link or ABI compatibility.
This separates renderer/audio bridge work from already supplied platform imports
and the unresolved common engine globals without introducing fake stubs.

MSVC string compatibility: added _stricmp/_strnicmp following the upstream native
port's unsigned-byte/ctype comparison behavior, including zero-length bounded
comparison without dereferencing input. Local platform v12 includes case, prefix,
count boundary and high-byte checks. Build/link pass, hardware untested. Formatted
output remains unresolved: do not alias legacy _snprintf/_vsnprintf directly to
C99 functions without auditing truncation, I64/I32 and narrow/wide specifiers.
Staged v8 unchanged. These compatibility helpers are not FPS optimizations.

Legacy narrow formatting bridge (local platform test v13): crt_format.c implements
_snprintf/_vsnprintf with conversion-aware I64/I32 translation, preserved literal
and escaped-percent text, and legacy count-byte truncation/exact-fit semantics.
Formatting evaluates the argument list once (including %n); temporary count+1
storage avoids silently losing the final payload byte. Unsupported wide formats,
pointer presentation, long double and unaudited length modifiers return EINVAL
without writing the destination, rather than crossing the 16/32-bit wchar ABI.
This is an initial narrow compatibility implementation, not complete MSVC CRT
coverage or a performance optimization; it currently allocates temporary buffers.
Reference semantics: Microsoft legacy snprintf/vsnprintf documentation.

Platform v13 includes below-limit/exact-fit/overflow canaries, zero-count cases,
64/32-bit integer formatting, escaped text, dynamic width/precision, %n, narrow
h-qualified characters/strings, and rejected wide/malformed formats. Cross-build,
full test-app link (no unresolved symbols), packaging and Clang static analysis
pass. First link exposed gnu89 va_copy availability; explicit compiler va-copy
builtin fixes it. Runtime tests have NOT executed. v13 is local only; staged v8
and installed Halo remain unchanged. Vita awake lease renewed successfully.
Evidence: build/vita/platform-v13-build.log, platform-test-link.json,
platform-package.json, crt-format-analysis.log.

Native engine arena integration (platform test v14): HALO_VITA branches in
source/cache/physical_memory_map.c now allocate all four Vita-owned arenas before
publishing engine addresses, verify owner identity and configured sizes, and
clear engine pointers after successful disposal. Failure aborts startup instead
of continuing with fixed Xbox pointers. Disposal refuses GPU-mapped arenas via
the existing owner. Native includes exclude the Xbox D3D inline headers; nm-u
shows only arena APIs, abort and memset (no Xbox physical-allocation imports).
Verification here checks allocation ownership, not Xbox page-protection policy.
No save/tag relocation, automatic GPU mapping or cache publication is implied.

Platform test v14 links that actual engine unit and adds base identity, first/last
byte writes, cleanup/null pointers, repeated free and reinitialization checks.
Cross compilation, full app link and packaging pass. Runtime still unverified:
Vita report retrieval returned FTP 550 File not found. Staged the separate test
as ux0:data/xita-platform-test-v14.vpk, full FTP readback verified SHA256
cad66fda76b36eea0dca9e620ca8a185fb135180760279bc916931173a1bb2d9,
138751 bytes. Manual install/launch needed; Xita remote cannot install a separate
title. Installed Halo and saves unchanged. Receipt:
build/vita/platform-test-v14-staged.json. This is startup integration, no FPS claim.

Game-state arena ownership follow-up: native game_state_allocate_buffer now checks
address identity, page alignment and overflow-safe CPU/GPU bounds. The GPU tail
remains cached CPU storage, with explicit future renderer publication required;
no fake Xbox write-combine protection call is used. Native game_state_free_buffer
releases its borrowed reference, leaving allocation disposal to the arena owner.
Three affected engine units cross-compile; game-state object no longer imports
XPhysicalProtect/XPhysicalFree. Full shutdown/worker-retirement integration remains
open (shell_dispose currently omits game-state and physical pool disposal).
No hardware qualification, game update or FPS result. Staged v14 is unchanged.

Native decompressor shutdown: added an idle-boundary exit flag and dispose hook
that waits for current copy completion, joins the worker and closes owned handles.
Raw I/O helpers cannot safely accept mid-read cancellation, so disposal allows the
current copy to finish. Both affected cache units cross-compile. The separate
cache-read worker still needs a shutdown path; no arena-free shutdown wiring or
hardware qualification is claimed. Details in memory-bringup.md.

Native cache-read retirement: cache_files_dispose now joins the awakened reader
before freeing requests; it enforces map-close/drain preconditions, rejects live
requests at exit, and closes thread/event handles after join. Init zeroes the
request array and rejects double initialization. Typed WINAPI entry avoids the
old mismatched function-pointer cast. Cross compile passes; runtime untested.
Existing request publication and busy-spin flushing remain to audit on ARM.

Vita cache request handoff now publishes fully initialized slots and retires them
with release/acquire pending flags; priority promotion is atomic. Completion-event
waits replace full-pool/map-close spinning under the existing single-producer,
single-waiter contract. Cross-build and generated ARM barriers checked. Static
analysis still reports two existing assertion-default uninitialized paths; no
runtime pass claimed. External texture/sound completion flags remain to audit.

Completion publication follow-up: shared cache_flags.h now applies release/acquire
semantics to cache callbacks and texture, sound, tag and BSP consumer flags; all
cache_file_read call sites audited. Four units cross-compile. CPU payload readiness
is covered by the code change, but runtime/lifetime stress tests and GPU/audio
cache visibility are still pending. No game deployment or FPS claim.

SetEndOfFile startup dependency: file_position.c now resizes to the current file
position using ftruncate while holding the same per-file I/O lock as seek and
transfers. Retains the typed handle, rejects non-writable handles, retries EINTR
and propagates OS errors. Callers must drain queued async operations when ordering
against resize matters. No map/save files outside the isolated native tree touched.
Local platform v15 adds truncate, extend, retained-prefix, unchanged-position,
zero-size, stale-handle and read-only rejection checks. It intentionally makes no
assumption about extended-byte contents. Full cross-build/link/package passes;
no execution performed. Staged v14 and installed Halo remain unchanged.
Evidence: build/vita/platform-v15-build.log and platform-test-link.json.

Cumulative rebuild after cache publication/file-resize changes: fresh --engine
receipt build/vita/compile-receipt-20260928T180851.901546Z.json still reports
465/467 compiled, with only x86 stack_walk_windows.c and absent linker_common.c
failing. Fresh platform/adapter compilation and relocatable link join 495 objects,
539 unresolved symbols and zero weak unresolved references. This is NOT a linked
executable. Dependency inventory: 160 symbols have SDK/library providers,
110 Xbox graphics, 41 Xbox audio, 11 Bink, 217 require owner review. Archive export
matches do not prove ABI compatibility. No new compile failures observed.

Startup-order finding: shell_xbox.main calls rasterizer preinitialize before
physical_memory_allocate. That routine creates Direct3D, creates a device and
backbuffer/depth configuration, queries caps, presents once, and releases device
and Direct3D before normal rasterizer initialization later creates resources.
Therefore the next major runtime boundary is an actual GXM device/display lifecycle
supporting create/present/release/recreate, not more incidental CRT stubs. The
current native texture upload test performs no draws and is insufficient evidence
for that boundary. Existing Xita backend is guest-state dependent; isolate reusable
GXM operations without inventing D3D success stubs or claiming engine launch.

Vita platform-test report was rechecked this turn: FTP 550 File not found. Manual
separate-app execution remains pending; no physical test or new FPS measurement.

Native graphics context ownership (local platform v16): graphics_context.c creates
three uncached mapped command rings and a fragment-USSE ring, allocates context
host memory, creates a real GXM immediate context and one-scene non-MSAA render
target. No guest runtime state or fake D3D object is involved. Width/height and
double-init checks run before allocation; failure cleanup retains failed-release
ownership for retry. Destroy finishes the context only at shutdown, destroys
target/context, unmaps rings and frees memory in dependency order. No frame-loop
full-GPU wait is introduced. GXM initialization is explicitly caller-owned.

Local v16 tests invalid dimensions, create/access, double-init rejection,
destroy/null getters, repeated destroy and recreate at 640x480 then 960x544,
inside the existing GXM-initialized test section. Cross compile, full standalone
link/package and Clang static analysis pass; hardware execution is pending.
This supplies context/target ownership only: display buffers/queue/sync objects,
color/depth surfaces, shaders and draw submission are not yet connected. Thus it
does not implement D3D CreateDevice/Present or launch/render Halo. Staged v14 is
unchanged, installed perf307 untouched. Evidence: platform-v16-build.log,
platform-test-link.json, graphics-context-analysis.log.

Native surface ownership (local platform v17): graphics_surfaces.c allocates three
color/depth pairs in 256-KiB-aligned CDRAM allocations, maps them READ|WRITE, and
creates linear ABGR color surfaces, tile-aligned S8D24 depth surfaces and one GXM
sync object per slot. Bounded dimensions (up to 960x544), 64-pixel color stride,
initial zero fill/write barrier, partial-failure cleanup and retained failed-free
ownership are explicit. Pins reject destruction while published references exist;
caller retirement must follow actual GPU/display completion. This is storage and
ownership, NOT a implemented presentation/fence polling loop or GPU-ready reuse
proof. Lower-resolution surfaces require a future upscale path before scanout.

Local v17 adds dimensions/double-init, three descriptors, pin/refuse-destroy,
retirement, invalid-slot and recreate checks while GXM is initialized. Cross build,
full standalone link/package and static analysis pass; hardware execution remains
pending. No draw/display queue entry has been submitted, no D3D device bridge
connected. Staged v14 and installed perf307 unchanged. Evidence:
platform-v17-build.log, platform-test-link.json, graphics-surfaces-analysis.log.

Native presentation (platform v18): graphics_present.c configures an exclusive
GXM display queue callback, pins a bootstrap old buffer, acquires only free/retired
slots and wraps BeginScene/EndScene/PadHeartbeat/DisplayQueueAddEntry. Callback
sets scanout, waits vblank and release-publishes old-slot retirement. Only the
render owner unpins/reuses slots after acquire observation. No available slot
returns busy=1; queue capacity itself may still block in the GXM driver. Callback,
heartbeat and queue errors keep pins and stop new submission. An EndScene failure
leaves the scene active and refuses teardown (no guessed safe destruction).

Shutdown only uses GXM Finish/DisplayQueueFinish, disables scanout, waits two
vblanks, then retires pins. Normal frames contain no full-GPU Finish. Assumes
exclusive queue/context/surface lifecycle and 960x544 scanout; no upscale yet.
All resources must outlive presentation shutdown. It is not the D3D device adapter.

Platform v18 configures the real callback and adds six empty Begin/End/flip scenes,
slot reuse, busy polling with a test deadline, active-scene rejection, pin-protected
destruction and scanout detach before free. Uses default GXM parameter-buffer size.
No shaders or draw calls: this is lifecycle/queue testing, not visual game proof.
Cross build/link/package and static analysis pass, runtime still untested. Staged
ux0:data/xita-platform-test-v18.vpk with full FTP readback verification; receipt
build/vita/platform-test-v18-staged.json. Manual install/launch required. Existing
v14 package retained and installed Halo/saves unchanged. No FPS result.

Native shader patcher ownership (local platform v19): shader_patcher.c supplies
512 KiB mapped patcher storage plus 256 KiB vertex/fragment USSE pools, host
allocation callbacks, real GXM patcher creation and retryable teardown. Explicit
registered-program-owner references prevent pool destruction while retained.
Callers must GPU-retire and release/unregister every program before dropping their
reference; the pool cannot infer program lifetime from raw GXM calls. No automatic
shader binding or translated register dependency is introduced.

Local v19 adds patcher create/double-init, retained destroy rejection, release,
null-getter and recreation cases while GXM is initialized. Cross build/link/package
and static analysis pass, runtime untested. Existing Xita shaders/xv_clear.gxp and
xv_color.frag.gxp are present alongside their runtime-written Cg sources; these
are the next clear-program candidates, not original Halo program replacements.
No program has been registered/drawn by this native path yet. Staged v18 and
installed Halo unchanged. Evidence: platform-v19-build.log,
shader-patcher-analysis.log, platform-test-link.json.

Shader-backed draw (platform v20): embed_clear_shaders.py embeds the explicitly
selected Xita runtime-written clear VS/color FS binaries into private build output,
recording hashes (clear-shader-inputs.json); reuse retains Xita licensing. clear_draw.c
validates GXP, registers/patches programs and binds float3 position + normalized
byte color attributes. Immutable uncached mapped triangle/index data covers the
viewport. The draw sets culling/depth state explicitly and submits sceGxmDraw.
This is a fixed-color bring-up draw, not full D3D clear semantics or Halo rendering.
Program resources retain the patcher until release/unregister, with GPU finish
only during shutdown before freeing geometry. Scene/lifecycle owner contracts apply.

v20 runs six shader-backed scene flips then, after GPU/display drain, checks three
pixels per surface against ABGR 0xff804020. Acquisition now rotates its starting
slot to exercise all three buffers rather than repeatedly favoring low indices.
Checks are COMPILED, NOT EXECUTED. Cross build/link/package/static analysis pass.
A conversion failure exposed insufficient text/data gap for SCE metadata;
platform_test.ld reserves an extra page before writable data. Verified ELF gap
and successful vita-elf-create/package after the fix; no warning suppression.

Staged ux0:data/xita-platform-test-v20.vpk with full FTP readback verification
(platform-test-v20-staged.json). Older test packages retained. Manual separate-app
installation/launch pending; no native rendered screenshot or hardware FPS claim.
Installed Halo and saves unchanged. build/vita/platform-v20-build.log and
clear-draw-analysis.log hold build/static results. audit_link.py also generates
shader inputs before compiling the platform modules.

Integrated native device owner (local platform v21): graphics_device.c now owns
GXM init, native context/target, three surface slots, shader patcher, clear program
and presenter. Stage tracking includes attempted partial initialization so cleanup
can retry retained resources. Failure leaves prerequisites alive rather than
terminating GXM beneath live handles. Shutdown detaches presentation before
program/pool/surface/context destruction and GXM termination. A test-frame method
closes its scene even when the draw reports failure. Single render owner and
exclusive GXM lifetime remain required; this is NOT an Xbox D3D object/caps shim.

Local v21 adds two full create/draw/present/destroy/recreate cycles after the
component tests terminate GXM, plus double-init/uninitialized draw rejection.
Cross-build/link/package and static analysis pass; hardware pending. No engine
startup path claims D3D support from this owner yet. Staged v20 and installed
perf307 unchanged. Evidence: platform-v21-build.log, graphics-device-analysis.log.

Native textured draw (local platform v22): texture_draw.c connects the bounded
texture decoder/upload path to Xita's runtime-written xv_test_tex VS and xv_tex0
FS. GPU vertex uniforms supply identity transform; fragment uniforms explicitly
disable alpha testing. UV/position/color attributes, sampler index, point filtering
and clamp modes are bound explicitly. Immutable geometry and uploaded texture
remain pinned until GPU-finished destruction after presentation shutdown.
Programs/texture resources unwind before releasing the retained patcher owner.
This is a bring-up single-mip draw, not the Halo material/shader bridge.

Local v22 adds six textured scene flips with a 4x4 four-quadrant ARGB input, decoded
and uploaded through the native path. After GPU/display drain, per-slot four-point
readback checks red/green/blue/white orientation and channels. These checks compile
but have NOT executed. Full build/link/package/static analysis pass. Embedded
runtime shader inputs remain private generated output with recorded hashes and
Xita licensing requirements. Staged v20 and installed Halo unchanged; no native
rendered screenshot or FPS result. Evidence: platform-v22-build.log,
texture-draw-analysis.log, clear-shader-inputs.json.

Retail texture fixture preparation (2026-09-28): extract_bitmap_fixture.py now
extracts an explicitly selected small 2D DXT chain from a validated retail 2276
map. Checks include bounded typed records, local tag ownership, signature,
power-of-two dimensions, hardware chain size, and pre-tag pixel span. Output
requires a new directory and remains private, outside the source tree/package.
Six synthetic parser tests pass, including malformed size/pointer/payload cases.
Actual b30 tag 6 bitmap 0 (128x128, format 15, 21888 bytes) extracted to
../private-texture-fixtures/b30-sky-6 with map/reader/payload SHA256 provenance.
This is offline asset parsing only: decoder contents, GPU sampling and native
engine retail compatibility remain unverified. Next: explicit optional fixture
input in the standalone texture test, preserving the known-color channel checks.
No new VPK staged or Halo update deployed; perf307 remains installed.

Local platform v23 connects the optional private fixture to real texture draws.
Build with --private-texture-fixture ../private-texture-fixtures/b30-sky-6 to
produce xita-platform-test-private.vpk (contains owned pixels; never distribute).
The embed step validates metadata, SHA256, dimensions and chain size. Default
build explicitly disables the fixture and produces xita-platform-test.vpk.
Both variants cross-compile, link without undefined symbols, and package.
Private provenance retained in build/vita/platform-v23-private-package.json;
default fixture exclusion checked after rebuilding without the option.

Runtime test keeps synthetic quadrant checks, then selects retail mip 0, decodes
it and samples through GXM across six flips/three slots. After GPU/display drain,
four point-filter samples per slot are compared with CPU decoder output. This
checks upload/sampling consistency, NOT independent decoder correctness. Both
variants remain hardware-unexecuted; staged v20 and installed perf307 unchanged.

v23-private staged with full byte readback verification; receipt
build/vita/platform-v23-private-staged.json. Runtime report still absent.
See texture-integration.md for cached descriptor registration-before-I/O ordering,
first safe conversion point, LRU identity/retirement requirements and separate
managed/render-target paths. No D3D bridge or hardware success claimed.

Local platform v24 adds texture_resource.c: a render-owner sidecar for one
selected mip/face. Registration validates the view without reading pixels;
prepare acquire-loads the I/O completion byte before decoding/uploading once.
Generation tokens reject stale access after reuse and refuse wraparound.
Pins are required to expose the GXM descriptor; pending I/O or GPU pins prevent
unregistration. Failed preparation retains partial cleanup ownership until
unregistration. Caller must retain the completion byte/source, publish with
release ordering, keep loaded bytes immutable, and retire only completed GPU
references. Sidecars must be initialized once, not memset on datum reuse.

v24 tests cover post-registration data arrival, pending prepare, cached prepare,
pinned deletion, stale tokens and generation exhaustion. Cross-build/full link/
package and clang static analysis pass; hardware tests remain unexecuted.
This is not connected to engine cache binding yet and represents one mip/face,
not a full mip-chain/cube/render-target implementation. Staged v23-private and
installed perf307 unchanged. Evidence: platform-v24-build.log and
texture-resource-analysis.log.

Local platform v25 wires registered texture resources into texture_draw.c.
create_resource prepares once, returns pending before draw allocation if I/O is
unfinished, pins the shared upload and keeps its generation token. GXM sampler
settings use a copied descriptor so shared resource state is not changed. Teardown
retires the borrowed pin only after the existing shutdown GPU drain; it leaves
actual resource destruction to the cache owner. Direct-view drawing remains
available and owns its own upload. This bring-up helper still retains one pin
for its lifetime, not per-scene retirement in a production cache.

The deferred resource test now submits six real textured scenes, checks the
center pixel in all three retired surfaces, checks upload reuse and deletion
refusal while borrowed, then releases the draw and unregisters the resource.
Build/link/package and static analysis pass; GPU checks have not executed.
Evidence: platform-v25-build.log, texture-draw-v25-analysis.log. No perf307
change or hardware performance claim; staged v23-private remains available.

Local platform v26 adds per-scene texture references to graphics_present.c.
Each of the three slots records at most 512 unique sidecar/token pairs. Draw
submission pins before GXM use; repeated use in one scene is deduplicated. A full
list returns error before submission, never silently drops ownership. On slot
retirement the render owner releases pins after acquiring the existing callback
retirement state (GPU/display completion); callback threads never mutate texture
objects. Failed queue operations retain references for shutdown drain. Failed
retirement keeps remaining entries, avoiding untracked cleanup.

Borrowed-resource texture draws now register scene use. The test checks no-scene
rejection, one additional pin per scene despite repeated use/submission, bounded
live references across six flips, and remaining helper-only pin after shutdown.
The bring-up helper still holds a lifetime pin until destruction; production
cache users will own resources independently and use per-scene pins directly.
No new per-frame Finish added. Existing display-queue capacity can still block;
this is not a claim of zero render-thread waits or prompt GPU-only retirement.

Cross-build/full link/package and static analysis pass; runtime assertions remain
unexecuted. Evidence: platform-v26-build.log, graphics_present-v26-analysis.log,
texture_draw-v26-analysis.log. Installed Halo perf307 and staged v23 unchanged.

Local platform v27 preserves compression for eligible registered DXT1/3/5 mips
(2D power-of-two dimensions 4..4096). The bounded adapter reuses Xita's licensed
BC block reorder helper; native upload allocates uncached storage, reorders once,
maps READ and creates a GXM swizzled UBC1/2/3 descriptor. No CPU RGBA temporary
or decompression is used on this path. Smaller mip views retain decoded fallback.
This is one selected mip, not full-chain support, and does not prove GXM sampling.

The existing resource draw test now samples a compressed red DXT1 block. Added
rectangular 16x8 block-order test with independently enumerated destination order,
output sentinel, short buffer and alias rejection. All tests are hardware-pending.
Cross-build/full link/package and upload/resource static analysis pass. Evidence:
platform-v27-build.log, texture_upload-v27-analysis.log,
texture_resource-v27-analysis.log.

For the private 128x128 DXT3 fixture, mip-0 storage is 16384 bytes instead of
65536 decoded bytes (both before other resource overhead). Installed Xita already
has a compressed path; this brings that capability into the native prototype,
not a new perf307 optimization. No FPS claim or hardware deployment made.

Local platform v28 adds an EXPLICIT experimental square DXT mip-chain upload.
Source runtime xv_ui_gxm.c documents suspected historical GPU faults with BC
mips and keeps its mip option disabled by default; do not infer qualification
from code reuse. Native resource preparation therefore remains single-mip.
The new function bounds every selected source mip, reorders each into consecutive
GXM blocks, publishes uncached storage and creates the mip descriptor. It accepts
only square 2D chains ending at 4x4; rectangular/cube/full material support remains
open. Test checks short payload rejection, two-level descriptor/storage and
rectangular rejection, not shader sampling at minification. Cross-build/link/
package and static analysis pass; runtime is still unexecuted.

Fresh full-engine compile/link audit (2026-09-28): 465/467 engine units compile,
with stack_walk_windows.c x86 inline assembly and absent linker_common.c still
failing. Relocatable link now includes 503 objects and 591 unresolved symbols,
zero weak unresolved. Expanded SDK provider scan includes Display and the actual
ARM hard-float compiler-runtime archive: 222 candidate library providers,
110 Xbox graphics, 41 audio, 11 Bink, 207 remaining owner-review symbols.
Raw count increased as real GXM implementation added SDK calls; it is not a
performance metric or a count of missing game functions. No native game binary.
prototype.json refreshed to current evidence (older historical progress retained).

Next platform dependency: cache_files_windows.c stores its cache-recency marker
through the FILETIME CREATION slot, despite naming it last_modification_date.
Read uses GetFileTime's first output; update uses SetFileTime's first input.
Do not emulate these using POSIX modification/change time. Vita SceIoStat
st_ctime explicitly represents creation time; native implementation needs exact
Windows epoch/unit conversion, validity checks, retained file handle ownership,
and creation-time writes via the appropriate Vita stat flag. CompareFileTime
must compare unsigned 64-bit timestamps. No timestamp shim added in this audit.

Local platform v29 implements CompareFileTime (unsigned 64-bit ordering),
SystemTimeToFileTime (validated UTC date, ignored weekday, RTC Windows epoch
conversion), and GetSystemTime (UTC clock, millisecond truncation). Invalid
conversion leaves output untouched. GetSystemTime aborts on unexpected RTC
failure because its Windows signature cannot report failure. SceRtc stub is now
an explicit test-link/provider dependency. Compiled tests cover epoch1970,
123ms, low-word carry ordering, invalid1900 leap day, valid2000 leap day and
current-time conversion. Full cross-build/link/package pass; hardware pending.

GetFileTime/SetFileTime are NOT implemented yet. Disassembly of installed newlib
_fstat_r proves POSIX fd is translated through __vita_fd_grab then the retained
entry's SceUID is passed to sceIoGetstatByFd, followed by __vita_fd_drop. Passing
file->fd directly is incorrect. No private struct layout guessed; resolve a
maintainable native handle access path before implementing creation-time I/O.
Evidence: build/vita/newlib-syscalls.o and platform-v29-build.log. Staged v28 and
installed perf307 unchanged.

Local platform v30 adds GetFileTime/SetFileTime using creation/access/write
SceIoStat fields through a retained open descriptor. Newlib's MIT-licensed
vitadescriptor.h is vendored unchanged at upstream commit
2e428297c0b6aefd830c5a75a7daa7e774562a42, with license and provenance retained.
Only newlib_descriptor.c uses its internal ABI, compiled with short enums to
match the installed SDK; offset/enum-size assertions and SHA256 checks of the
installed io/syscalls archive members reject unaudited toolchain changes.
No filename reopening and no POSIX-fd-as-kernel-UID assumption.

File handles and I/O locks remain held through kernel operations. Get converts
requested outputs before publishing any, Set validates all requested fields
before writing selected flags. Read-only handles reject Set; NULL fields remain
unchanged. All-ones update-suppression sentinel explicitly rejects (unsupported),
kernel failures report generic Win32 I/O failure; filesystem timestamp precision
still applies. Tests exercise three distinct dates, creation-only update retaining
other dates, stale handles and read-only rejection using isolated test files.
Cross-build/full link/package and static analysis pass; physical filesystem tests
have NOT run. Staged v28 and installed Halo remain unchanged.

Local platform v31 removes the native stack-walk compile blocker. HALO_VITA
stack_walk_with_context calls a bounded ARM EHABI _Unwind_Backtrace collector,
prints raw current-thread PCs, and explicitly labels supplied Xbox CONTEXT as
not interpreted. It neither reads x86 EBP/EIP nor applies x86 call-displacement
arithmetic to ARM instructions. Native initialization skips the Xbox linker-map
symbol file. Original Xbox/Linux behavior remains under its existing branches.
Only real unwound PCs are returned; zero frames is possible and not fabricated.

Native compile flags now emit unwind tables. Standalone test has noinline nested
frames, output bounds/sentinel checks and a minimum-two-frames assertion. Build,
full test link/package and targeted stack_walk_windows.c compilation pass; ELF
contains ARM.exidx/ARM.extab. Hardware unwinding is unexecuted, especially across
library code without unwind information. An Xbox exception context is still not
a native fault-context unwinder. Crash termination paths were not changed.
Evidence: platform-v31-build.log and stack-walk-vita-build.log. Last full-engine
receipt predates this fix; do not label the whole engine executable from it.

Full native compile milestone (2026-09-28): 466/466 actual engine C units now
compile. source/linker_common.c is documented upstream as pooled COMMON metadata,
not a translation unit; engine_units.py excludes only that specific pseudo-unit,
with a recorded reason. This does not hide unresolved globals: they remain in
aggregate link reports until real typed definitions exist.

Native common_storage.c defines twelve data-array pointers and error_globals
using exact reconstructed types. dsound_globals, sound_channels and wind_globals
are tentative HALO_VITA definitions in the owning source units where their full
structs/array sizes are known. No weak byte arrays, guessed symbol spacing or
Linux fallback functions imported. Existing Xbox declarations remain unchanged.

Fresh aggregate: 509 objects, 579 raw unresolved, zero unresolved weak symbols.
235 candidate SDK/CRT/compiler providers; remaining 344 = 110 graphics + 41 audio
+ 11 Bink + 182 owner-review. Still no native game executable or hardware gain.
Evidence: engine-native-common-build.log, native-common-link.log,
partial-link-receipt.json, link-dependencies.json. prototype.json refreshed.

Second typed COMMON pass: 49 missing globals are now HALO_VITA tentative
owner-unit definitions (AI/game state, exact shader/state structs and arrays,
UI cache entries, audio/lighting pointers, network key data). Declaration and
owner manifest: common-owners.json. No initialized asset data, guessed byte
storage or stubbed functions added. Existing other-platform extern declarations
remain intact. Full 466-unit compile passes; aggregate symbol check proves all
49 are non-weak COMMON definitions with compiler-derived sizes.

Fresh aggregate drops raw unresolved 579 -> 530, with candidate library providers
unchanged at 235: 295 non-library references remain (110 graphics,41 audio,11
Bink,133 owner-review). This is link progress, not runtime or performance proof.
Evidence: engine-common-owners-build.log, common-owners-link.log,
common-owners-symbol-check.json and current link-dependencies.json.

Local platform v32 adds ARM FPSCR control for the engine's sole _control87 call
(real_math_reset_precision: CW_DEFAULT,0xfffff). Rounding modes map to ARM's
reversed up/down encoding; exception masks/status translate explicitly. Only
masked exceptions and the PC_53 startup request are supported; other precision
or trap requests abort rather than pretend success. PC_53 is a compatibility
selector: ARM float/double still use C type precision, NOT x87 precision-control
emulation. Existing FZ/default-NaN/unrelated FPSCR state is preserved.

fast_ftol_C now uses VCVTR (current rounding mode) with explicit x87 integer-
indefinite INT32_MIN/invalid flag for NaN/infinity/out-of-range, since native VFP
invalid results differ. Tests cover ties-to-even, all four directions, int bounds,
NaN and clearing sticky invalid. Hardware tests remain unexecuted. Build/link/
package/static analysis pass; disassembly confirms VMRS/VMSR/VCVTR instructions.
This fixes native math dependencies, not proof of full math equivalence or FPS.
Evidence: platform-v32-build.log and float-control-analysis.log. No Halo update.

Local platform v33 initializes the supported CW_DEFAULT and clears sticky FP
status in the CreateThread wrapper immediately before the game start routine.
This avoids depending on pthread inheritance or the creator's current rounding.
No global emulated control word is shared; actual FPSCR belongs to each thread.
Direct internal pthread I/O helpers are unchanged (no game math dispatched there).

New two-thread test sets parent rounding DOWN, verifies child starts NEAREST,
changes child to UP and raises invalid, then handshakes through events and joins.
It checks parent mode/status remain independent and child state survives waiting.
Cross-build/full link/package pass; scheduler FP context isolation is NOT proven
until this runs on hardware. Current FTP report lookup still returns 550 missing.
Evidence: platform-v33-build.log. Staged v28 and installed Halo unchanged.

Native startup reachability audit: compile emits function/data sections, then
an explicit main-rooted relocatable GC link. audit_startup_link.py verifies main,
main_loop and shell_initialize remain defined. elf_references.py reads bounded
ELF32 relocation records and counts only imports referenced by SHF_ALLOC sections.
Important: nm -u on ld -r output retained all 528 undefined names even when users
were discarded; counting those names alone was not a valid reachability result.
Surviving loaded sections reference 448 imports, with 80 aggregate-only names.

GXM prototype helpers disappear from this startup-rooted graph: this confirms
that they are NOT yet wired into engine D3D initialization/submission. The graph
also omits some tooling helpers. Missing APIs used by reachable code still need
real implementations. This audit permits unresolved imports and does not produce
an executable, prove dynamic behavior, or measure FPS. Evidence: engine-sections-
build.log, sections-link.log, startup-link.map, startup-link-receipt.json and
startup-link-unresolved.txt (the latter is raw nm; use receipt for actual refs).

### Device integration audit (2026-09-28)

See [device-integration.md](device-integration.md). The engine has two device
lifetimes and directly constructs backbuffer/depth texture aliases. The next
bridge prerequisite is allocation/alias ownership and resolution-correct
presentation, not simply forwarding CreateDevice to the clear-frame helper.
No new build was deployed and no native runtime or FPS result is established.

### Platform test v34: surface allocation ownership

Added a borrowed-allocation registry and connected it to the three native
color/depth slots. Copied views and GPU pins independently prevent surface
cleanup. Built the standalone v34 test with no unresolved link symbols;
hardware execution remains unverified. See device-integration.md for scope
and the remaining D3D header/view bridge. Installed Halo remains perf307.

### Platform test v35: D3D surface/texture header decoding

Added checked pixel-container decoding and retained allocation lookup. ARGB
and ABGR are distinct; invalid spans and unsupported depth/cube/mip layouts
are rejected. The standalone package links successfully; physical runtime
validation and engine D3D entry-point integration remain pending. No installed
Halo change or new performance result.

### Platform test v36: zero-copy color descriptors

Added GXM color texture descriptors over checked native D3D views, with ARGB
and ABGR channel layouts preserved. Presentation slot reuse now also checks
outstanding GPU references to the allocation. Standalone build/link succeeded;
physical testing and automatic scene ownership for these views remain pending.

### Platform test v37: scene-retired zero-copy draws

Connected allocation GPU pins to scene retirement and added D3D-header-backed
GXM texture submission to the draw helper. Built a six-frame ARGB-to-ABGR
readback test with feedback/lifetime checks. Package staged and readback
verified at ux0:data/xita-platform-test-v37.vpk; not installed or executed.
Halo remains perf307. Engine D3D integration is still incomplete.

### Platform test v38: native D3D surface metadata

Added checked surface-header export and real engine-facing GetDesc/LevelDesc
implementations for registered single-level linear color resources. Corrected
surface zero-mip-count handling. Standalone build and engine partial-link audit
succeeded; hardware validation is still pending. v37 is the latest staged VPK.

### Platform test v39: managed D3D surface headers

Added bounded managed-header ownership and a checked D3DResource_Release path
for those headers. Standalone build/link succeeded; no hardware run. Engine
inspection confirms two cached primary-buffer aliases, requiring separation
from the prototype's three display slots before real device publication.

### Platform test v40: two logical targets and display scaling test

Added stable 640x480 ARGB/depth targets independent of triple display buffers.
Compiled a four-quadrant GPU scaling/readback test using the logical storage;
no physical pass has been observed. GPU-write-to-sampling transitions and the
actual D3D device remain unfinished. Local v40 built; latest staged is v37.

### Platform test v41: offscreen GPU completion

Added per-target fragment-notification polling and shared-context scene
ownership. Compiled an offscreen GPU render → notification → texture sampling
→ display readback test. No per-frame full-GPU wait was added. Local v41
build/link succeeded; physical execution is pending and v37 remains staged.

### Platform test v42: repeated logical/display reuse

Fixed retirement progress when a logical write precedes the next display
scene, and added source rebinding without shader/geometry recreation. Built
twelve alternating GPU render/sample cycles. Latest standalone test is staged
and readback-verified at ux0:data/xita-platform-test-v42.vpk; not installed/run.
No Halo deployment or new FPS measurement.

### Platform test v43: logical GetBackBuffer

Added managed logical backbuffer ownership and engine-facing GetBackBuffer.
Selection preserves two stable resource identities; caller refs prevent owner
teardown. Standalone build/link passed; hardware remains unverified. Local
v43 is not staged; v42 is the latest package on the Vita. perf307 is unchanged.

### Platform test v44: integrated native device buffers

Device creation/cleanup now owns logical buffers, backbuffer headers and the
scaling draw. Logical presentation advances only on successful submission.
Standalone build/link passed with recreation and retained-header cleanup tests;
no physical validation yet. Latest staged VPK is still v42.

### Platform test v45: engine D3D lifecycle wired

Implemented the engine's create/caps/present/release entry points for the
supported probe configuration. Standalone and partial-engine link audits pass;
startup reachability now retains the GXM device/presentation implementation.
Game draw/depth/state APIs remain unfinished and hardware execution is pending.
No Halo performance claim; latest staged platform package remains v42.

### Platform test v46: depth surface API

Added managed depth headers and GetDepthStencilSurface. Depth metadata is
kept distinct from native tiled storage; color-texture sampling rejects depth
views. Standalone build/link passed; physical testing and depth readback/effect
conversion remain pending. Local v46, staged v42, Halo still perf307.

### Platform test v47: primary render scenes and viewport

Added primary SetRenderTarget and SetViewport handling; Present closes the
logical scene before sampling. Engine startup link now retains logical scene
submission. Standalone and partial-link checks pass; no hardware pass or Halo
FPS gain is claimed. General clears/state/shader/draw handling remains open.

### Platform test v48: masked native clears

Added clear draws with Xbox channel masks, depth/stencil write selection and
per-logical-buffer vertex storage. Alpha-only fog clears preserve RGB in the
compiled readback test. Build/link passed; actual color/depth/stencil hardware
validation and general draw-state integration remain pending.

### Current shader input requirement (v72)

Native test builds now require an explicit `--shader-dir` alongside
`--xita-source`. Use the generated shader directory from the selected Xita
build, containing both `xv_layouts.h` and the GXP files. The source checkout's
`shaders/` directory is older than perf307's generated shader set. See
[device-integration.md](device-integration.md) for the current reproducible
command and qualification status. Building a VPK does not establish hardware
execution or a performance improvement.
