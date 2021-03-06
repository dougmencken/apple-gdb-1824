/*
 *     Generated by class-dump 3.3.4 (64 bit).
 *
 *     class-dump is Copyright (C) 1997-1998, 2000-2001, 2004-2011 by Steve Nygard.
 */

#import <DebuggerGDB/PBXLSLaunchSession.h>

@class DBGGDBLauncher, DBGGDBSession, NSMapTable, PBXLSDebuggerAdaptor<PBXLSRestartExecutableRequest>, PBXLSProcess;

@interface PBXLSDebuggingSession : PBXLSLaunchSession
{
    DBGGDBSession *_foundationDebugSession;
    PBXLSProcess *_process;
    PBXLSDebuggerAdaptor<PBXLSRestartExecutableRequest> *_restartExecAdaptor;
    BOOL _readyForBreakpointSequences;
    NSMapTable *_breakpointsToTokens;
    id <DVTObservingToken> _debugSessionStateObserverToken;
}

+ (BOOL)_isExceptionBreakpoint:(id)arg1;
+ (BOOL)_isSymbolicBreakpoint:(id)arg1;
+ (BOOL)_isFileBreakpoint:(id)arg1;
@property(retain, nonatomic) DBGGDBSession *foundationDebugSession; // @synthesize foundationDebugSession=_foundationDebugSession;
- (void)_ensureEnabledBreakpointsAreCreated;
- (void)_handleBreakpointLocationsRemoved:(id)arg1;
- (void)_handleBreakpointLocationsAdded:(id)arg1;
- (void)_handleBreakpointLocationChanges:(id)arg1;
- (void)_removeExceptionBreakpointObserversIfNeeded:(id)arg1;
- (void)_addExceptionBreakpointObserversIfNeeded:(id)arg1;
- (void)_removeSymbolicBreakpointObserversIfNeeded:(id)arg1;
- (void)_addSymbolicBreakpointObserversIfNeeded:(id)arg1;
- (void)_removeFileBreakpointObserversIfNeeded:(id)arg1;
- (void)_addFileBreakpointObserversIfNeeded:(id)arg1;
- (void)_removeBreakpointObservers:(id)arg1;
- (void)_addBreakpointObservers:(id)arg1;
- (void)observeValueForKeyPath:(id)arg1 ofObject:(id)arg2 change:(id)arg3 context:(void *)arg4;
- (BOOL)customDataFormattersEnabled;
- (void)sendLogMessageToDebuggers:(id)arg1;
- (void)_attemptDeleteOfBreakpoint:(id)arg1 forDebugger:(id)arg2;
- (void)_attemptCreateOfBreakpoint:(id)arg1 forDebugger:(id)arg2;
- (void)restartExecutable;
- (void)suspendExecutable;
- (void)syncWithDebugger;
- (void)debuggerDidInitialize:(id)arg1;
- (void)addInitialBreakpointsToDebugger:(id)arg1;
- (void)_addBreakpoints:(id)arg1 forDebugger:(id)arg2;
- (id)debuggerAdaptorForExecutableRestart;
- (void)registerRestartExecutableAdaptor:(id)arg1;
@property(readonly) DBGGDBLauncher *launcher; // @dynamic launcher;
- (id)process;
- (id)init;

@end

