/*  cl_debug 
    v1.0

    Copyright (c) 2003-2006, Cubelogic. All rights reserved.

    Redistribution and use in source and binary forms, with or without 
    modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice, 
      this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice, 
      this list of conditions and the following disclaimer in the documentation 
      and/or other materials provided with the distribution.
    * Neither the name of Cubelogic nor the names of its contributors may be 
      used to endorse or promote products derived from this software without 
      specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
    ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE 
    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
    POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef _cl_debug_h_
#define _cl_debug_h_
    
/* =============================================================================
    Cl_debug is a very light-weight C library for logging and debugging. The 
    only requirement is the C standard library (stdio.h and assert.h). 
    Additional macros for CoreFoundation and Cocoa are provided (on Mac OS X).
    This library also leverages the TRACE capabilities of the Adobe InDesign 
    CS2 SDK (plug-in development). Other per-app customizations could be added.

    This library is composed by the following files: 
        cl_debug.h, cl_debug.c.

    To use cl_debug you will need to provide a definition for this constant:
        extern const char *kOurProductName;

    The output is directed on stderr by default, but it can be hijacked to
    a separate file using:

        void cl_debug_init(const char *inLogFilePath);

    ** Logging Macros **

    The logging macros are meant for logging information that is desired in 
    production, so they should be used sparsely:
    
        LOG(<printf style C string>, ...);
        LOG0(<C string>);
        LOG_CF(const char *cStringNoFormatting, CFStringRef theStrToLog);
        LOG_NS(<NSString with NSString formatting>, ...);
        
    LOG() accepts a variable number of arguments, just like printf. LOG0() is
    provided for compilers w/ no gcc extensions (C99 doesn't allow an empty 
    variable argument list). LOG_CF() is a function that logs CFStrings with a
    C string prefix. LOG_NS() is a macro to log Cocoa objects descriptions. 
    Since these are macros (except LOG_CF), there's only one level of logging: 
    everything else is considered debugging.

    ** Debugging Macros **

    There are 3 levels of debugging, from low (0, finer internal details) to 
    high (2) priority. 

        debugNmsg(<printf style C string>, ...);
        debugNmsg0(<C string>);
        debugNcocoa(<NSString with NSString formatting>, ...);
        N = [0, 1, 2]

    debugNmsg(), debugNcocoa() accept a variable # of args, debugNmsg0() just 1.

    ------------------------------
    ** Preprocessor definitions **
    ------------------------------

    Cl_debug looks at the following preprocessor definitions to configure its 
    macros and determine which macros to enable:

    Per-app customizations:

        CL_DEBUG_USE_INDESIGN_SDK:
            If this preprocessor variable is defined, the TRACE macro 
            defined by the InDesign SDK will be used by cl_debug *on Windows* 
            for the debugging macros instead of OutputDebugString. On UN*X 
            machines debugging output will remain on stderr.
            
    Control switches:

        CL_DEBUG_DISABLE:
            This is the master switch: if defined, all debugging macros and 
            assertions are disabled.
            Note: Logging is always enabled.
      
        CL_DEBUG_ALWAYS_KEEP_ASSERTIONS:
            If defined, standard C/Obj-C assertions will be left in,
            even if CL_DEBUG_DISABLE is defined.
*/

/* ========================================================================== */

#if !defined(CL_DEBUG_DISABLE)

    /* Individual switches for different debug levels:
     * 0 is for internals debugging (lower priority), 2 is for higher level 
     * information (higher priority). */
    #define CL_DEBUG_0
    #define CL_DEBUG_1
    #define CL_DEBUG_2
    
    /* An individual switch to enable/disable unit testing. */
    #ifndef CL_UNIT_TESTING
        #define CL_UNIT_TESTING
    #endif
    
    /* An individual switch for the plugins. */
    #ifndef CL_PLUGIN_DEBUG
        #define CL_PLUGIN_DEBUG
    #endif
    
#endif

/* ============================================================================= 
 * Implementation
 */

#if !TARGET_OS_MAC
    #undef CL_DEBUG_INCLUDES_CF
#endif

#if defined(__COREFOUNDATION_COREFOUNDATION__) || defined(CL_DEBUG_INCLUDES_CF)
    #define USING_COREFOUNDATION
#endif

#ifdef CL_DEBUG_DISABLE
    #undef CL_DEBUG_0
    #undef CL_DEBUG_1
    #undef CL_DEBUG_2
    #undef CL_UNIT_TESTING
    #ifndef CL_DEBUG_ALWAYS_KEEP_ASSERTIONS
        /* Define NDEBUG to disable standard C assertions.
         * Define NS_BLOCK_ASSERTIONS to disable Cocoa NSAssertions.
         * NB: NDEBUG must appear before the #include <assert.h> */
        #ifndef NDEBUG
            #define NDEBUG
        #endif
        #ifndef NS_BLOCK_ASSERTIONS
            #define NS_BLOCK_ASSERTIONS
        #endif
    #endif
#endif

/* Provide a definition for this constant in every executable. */

#ifdef __cplusplus
    #include <cstdio>
    #include <cassert>
    extern "C" {
#else
    #include <stdio.h>
    #include <assert.h>
#endif

/* =============================================================================
 * stderr is used as default log file, but you can hijack it if you wish.    
 */

typedef struct 
{
    FILE *logfile;
    char logfile_path[1024];
} cl_debug_info;

extern cl_debug_info gCLDebugStat;

void cl_debug_init(const char *inLogFilePath);

/* ========================================================================== */

#ifdef USING_COREFOUNDATION
    #include <CoreFoundation/CoreFoundation.h>
#endif

/* eventually (Win): give option to redirect to OutputDebugString */
#define CL_LOG_OPEN()                                                          \
    if (gCLDebugStat.logfile_path && gCLDebugStat.logfile_path[0]) {           \
        gCLDebugStat.logfile = fopen(gCLDebugStat.logfile_path, "a");          \
        if (gCLDebugStat.logfile == NULL)                                      \
            gCLDebugStat.logfile = stderr;                                     \
    } else {                                                                   \
        gCLDebugStat.logfile = stderr;                                         \
    }
#define CL_LOG                                                                 \
    if (gCLDebugStat.logfile)                                                  \
        fprintf(gCLDebugStat.logfile, 
#define CL_LOG_CLOSE()                                                         \
    if (gCLDebugStat.logfile && gCLDebugStat.logfile != stderr &&              \
        gCLDebugStat.logfile != stdout) {                                      \
        fclose(gCLDebugStat.logfile);                                          \
    }

#ifdef WIN32
    #ifdef CL_DEBUG_USE_INDESIGN_SDK
        #define cl_debug_trace      TRACE
    #else
        #define cl_debug_trace      OutputDebugStringA
    #endif
    #define CL_DEBUG_BEGIN()        char buf[1024]
    #define CL_DEBUG_PRINT          sprintf(buf,
    #define CL_DEBUG_END()          cl_debug_trace(buf)
#else
    #define CL_DEBUG_BEGIN()    
    #define CL_DEBUG_PRINT          fprintf(stderr, 
    #define CL_DEBUG_END()
#endif

/* =============================================================================
 * Logging macros
 */

#define LOG(STF, ...)      do {                                                \
    CL_LOG_OPEN();                                                             \
    CL_LOG "%s: [%d] " STF "\n", kOurProductName, __LINE__, ## __VA_ARGS__);   \
    CL_LOG_CLOSE();                                                            \
} while (0)

#define LOG0(ST)           do {                                                \
    CL_LOG_OPEN();                                                             \
    CL_LOG "%s: [%d] %s\n", kOurProductName, __LINE__, ST);                    \
    CL_LOG_CLOSE();                                                            \
} while (0)

#if TARGET_OS_MAC && defined(FOUNDATION_EXPORT)
#define LOG_NS(STF, ...)   do {                                                \
    NSLog([@"[%d] %s: " stringByAppendingString: STF],                         \
          __LINE__, kOurProductName, ## __VA_ARGS__);                          \
} while (0)
#else
#define LOG_NS(STF, ...)
#endif

#ifdef USING_COREFOUNDATION
    #ifdef __cplusplus
    inline void LOG_CF(const char *cs, CFStringRef theStr) 
    {
        int len = CFStringGetLength(theStr) + 1;
        char *buf = (char *)malloc(sizeof(char) * len);
        CFStringGetCString(theStr, buf, len, kCFStringEncodingMacRoman);
        LOG("%s{%s}", cs, buf);
        free(buf);
    }
    #else
    void LOG_CF(const char *cs, CFStringRef theStr);
    #endif
#else
    #define LOG_CF(CS, CFS)
#endif

/* =============================================================================
 * Debugging macros
 */

#ifdef CL_DEBUG_0

#define debug0msg(STF, ...)      do {                                          \
    CL_DEBUG_BEGIN();                                                          \
    CL_DEBUG_PRINT  "#%s# %s[%d] " STF "\n",                                   \
            kOurProductName, __FILE__, __LINE__, ## __VA_ARGS__);              \
    CL_DEBUG_END();                                                            \
} while (0)

#define debug0msg0(ST)           do {                                          \
    CL_DEBUG_BEGIN();                                                          \
    CL_DEBUG_PRINT  "#%s# %s[%d] %s\n",                                        \
            kOurProductName, __FILE__, __LINE__, ST);                          \
    CL_DEBUG_END();                                                            \
} while (0)

#if TARGET_OS_MAC && defined(FOUNDATION_EXPORT)
#define debug0cocoa(STF, ...)    do {                                          \
    NSLog([@"#%s#[%d] " stringByAppendingString: STF],                         \
          kOurProductName, __LINE__, ## __VA_ARGS__);                          \
} while (0)
#else
#define debug0cocoa(STF, ...)
#endif

#define debug_enter(ST)      do {                                              \
    CL_DEBUG_BEGIN();                                                          \
    CL_DEBUG_PRINT  "#%s# %s[%d]::%s ENTER\n",                                 \
            kOurProductName, __FILE__, __LINE__, ST);                          \
    CL_DEBUG_END();                                                            \
} while (0)

#define debug_exit(ST)      do {                                               \
    CL_DEBUG_BEGIN();                                                          \
    CL_DEBUG_PRINT  "#%s# %s[%d]::%s EXIT\n",                                  \
            kOurProductName, __FILE__, __LINE__, ST);                          \
    CL_DEBUG_END();                                                            \
} while (0)

#else
#define debug0msg(STF, ...)
#define debug0msg0(ST)
#define debug0cocoa(STF, ...)
#define debug_enter(ST)
#define debug_exit(ST)
#endif

/* -------------------------------------------------------------------------- */

#ifdef CL_DEBUG_1

#define debug1msg(STF, ...)      do {                                          \
    CL_DEBUG_BEGIN();                                                          \
    CL_DEBUG_PRINT  "#%s# %s[%d] " STF "\n",                                   \
            kOurProductName, __FILE__, __LINE__, ## __VA_ARGS__);              \
    CL_DEBUG_END();                                                            \
} while (0)

#define debug1msg0(ST)           do {                                          \
    CL_DEBUG_BEGIN();                                                          \
    CL_DEBUG_PRINT  "#%s# %s[%d] %s\n",                                        \
            kOurProductName, __FILE__, __LINE__, ST);                          \
    CL_DEBUG_END();                                                            \
} while (0)

#if TARGET_OS_MAC && defined(FOUNDATION_EXPORT)
#define debug1cocoa(STF, ...)    do {                                          \
    NSLog([@"#%s#[%d] " stringByAppendingString: STF],                         \
          kOurProductName, __LINE__, ## __VA_ARGS__);                          \
} while (0)
#else
#define debug1cocoa(STF, ...)
#endif

#else
#define debug1msg(STF, ...)
#define debug1msg0(ST)
#define debug1cocoa(STF, ...)
#endif

/* -------------------------------------------------------------------------- */

#ifdef CL_DEBUG_2

#define debug2msg(STF, ...)      do {                                          \
    CL_DEBUG_BEGIN();                                                          \
    CL_DEBUG_PRINT  "#%s# %s[%d] " STF "\n",                                   \
            kOurProductName, __FILE__, __LINE__, ## __VA_ARGS__);              \
    CL_DEBUG_END();                                                            \
} while (0)

#define debug2msg0(ST)           do {                                          \
    CL_DEBUG_BEGIN();                                                          \
    CL_DEBUG_PRINT  "#%s# %s[%d] %s\n",                                        \
            kOurProductName, __FILE__, __LINE__, ST);                          \
    CL_DEBUG_END();                                                            \
} while (0)

#if TARGET_OS_MAC && defined(FOUNDATION_EXPORT)
#define debug2cocoa(STF, ...)    do {                                          \
    NSLog([@"#%s#[%d] " stringByAppendingString: STF],                         \
          kOurProductName, __LINE__, ## __VA_ARGS__);                          \
} while (0)
#else
#define debug2cocoa(STF, ...)
#endif

#else
#define debug2msg(STF, ...)
#define debug2msg0(ST)
#define debug2cocoa(STF, ...)
#endif

/* ========================================================================== */

#ifdef __cplusplus
}
#endif

#endif
