/*
 *     Generated by class-dump 3.3.4 (64 bit).
 *
 *     class-dump is Copyright (C) 1997-1998, 2000-2001, 2004-2011 by Steve Nygard.
 */

#import "PBXGDB_CommandSequence.h"

@class PBXGDB_ThreadController;

@interface PBXGDB_BaseThreadSequence : PBXGDB_CommandSequence
{
    PBXGDB_ThreadController *_threadController;
    unsigned long long _threadHandle;
}

- (unsigned long long)threadHandle;
- (void)setThreadHandle:(unsigned long long)arg1;
- (id)threadController;
- (void)setThreadController:(id)arg1;

@end

