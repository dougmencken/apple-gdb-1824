/*
 *     Generated by class-dump 3.3.4 (64 bit).
 *
 *     class-dump is Copyright (C) 1997-1998, 2000-2001, 2004-2011 by Steve Nygard.
 */

#import <DebuggerGDB/PBXGDB_CommandSequence.h>

@class NSString;

@interface PBXGDB_TemporaryBreakpointSequence : PBXGDB_CommandSequence
{
    NSString *_symbol;
}

+ (id)sequenceWithSymbol:(id)arg1;
- (void)execute;
- (void)_breakpointSet:(id)arg1;
- (void)_resultAllThreadsRunning:(id)arg1;
- (BOOL)handleErrorForMICommand:(id)arg1;

@end
