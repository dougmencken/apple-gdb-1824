/*
 *     Generated by class-dump 3.3.4 (64 bit).
 *
 *     class-dump is Copyright (C) 1997-1998, 2000-2001, 2004-2011 by Steve Nygard.
 */

#import "PBXGDB_UntilSequence.h"

@class NSString;

@interface PBXGDB_ContinueToCallSymbolSequence : PBXGDB_UntilSequence
{
    NSString *_symbol;
    NSString *_filename;
    unsigned long long _lineNumber;
    NSString *_topFunction;
    BOOL _initialStackRetrieved;
    BOOL _steppedIn;
    long long _maxStepAttempts;
}

+ (id)sequenceWithSymbol:(id)arg1 file:(id)arg2 lineNumber:(unsigned long long)arg3;
+ (long long)maxStepAttempts;
- (void)_checkPosition:(id)arg1;
- (BOOL)_symbolIsFunction:(id)arg1;
- (void)_resultAllThreadsStepping:(id)arg1;
- (void)_sendStepOutCommand;
- (void)_sendStepInCommand;
- (void)_stepFinished:(id)arg1;
- (void)_untilCommandConcluded:(id)arg1;
- (void)_sendCheckFrames;
- (void)_resultAllThreadsRunning:(id)arg1;
- (BOOL)handleErrorForMICommand:(id)arg1;
- (void)gdbStoppedWithResults:(id)arg1 onCommand:(id)arg2;
- (void)didFinish;
- (void)finalize;
- (void)dealloc;
- (id)init;

@end

