/*
 *     Generated by class-dump 3.3.4 (64 bit).
 *
 *     class-dump is Copyright (C) 1997-1998, 2000-2001, 2004-2011 by Steve Nygard.
 */

#import "PBXGDB_CommandSequence.h"

@class PBXGDB_MIMovePC;

@interface PBXGDB_MovePCSequence : PBXGDB_CommandSequence
{
    PBXGDB_MIMovePC *_movePCCommand;
}

+ (id)sequenceWithThreadID:(unsigned long long)arg1 fileSpec:(id)arg2;
- (id)miCommand;
- (void)setMICommand:(id)arg1;
- (void)didFinish;
- (void)execute;
- (BOOL)handleErrorForMICommand:(id)arg1;
- (void)dealloc;
- (void)_didFinish:(id)arg1;

@end

